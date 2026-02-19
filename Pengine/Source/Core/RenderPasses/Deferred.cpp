#include "../RenderPassManager.h"

#include "../Logger.h"
#include "../BindlessUniformWriter.h"
#include "../MaterialManager.h"
#include "../MeshManager.h"
#include "../SceneManager.h"
#include "../TextureManager.h"
#include "../Time.h"
#include "../FrustumCulling.h"
#include "../Raycast.h"
#include "../UIRenderer.h"
#include "../Profiler.h"
#include "../Timer.h"

#include "../../Components/Canvas.h"
#include "../../Components/Camera.h"
#include "../../Components/Decal.h"
#include "../../Components/DirectionalLight.h"
#include "../../Components/PointLight.h"
#include "../../Components/SpotLight.h"
#include "../../Components/Renderer3D.h"
#include "../../Components/SkeletalAnimator.h"
#include "../../Components/Transform.h"
#include "../../Graphics/Device.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/RenderView.h"
#include "../../Graphics/GraphicsPipeline.h"
#include "../../EventSystem/EventSystem.h"
#include "../../EventSystem/NextFrameEvent.h"

#include "../ViewportManager.h"
#include "../Viewport.h"

using namespace Pengine;

void RenderPassManager::CreateDeferred()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = Deferred;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(Deferred);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "Deferred.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const std::string lightsBufferName = "Lights";
		const std::shared_ptr<UniformWriter> lightsUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, lightsBufferName);
		const std::shared_ptr<Buffer> lightsBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			lightsUniformWriter,
			lightsBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		const Camera& camera = renderInfo.camera->GetComponent<Camera>();

		const GraphicsSettings& graphicsSettings = renderInfo.scene->GetGraphicsSettings();
		baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "brightnessThreshold", graphicsSettings.bloom.brightnessThreshold);

		auto directionalLightView = renderInfo.scene->GetRegistry().view<DirectionalLight>();
		if (!directionalLightView.empty())
		{
			const entt::entity& entity = directionalLightView.back();
			DirectionalLight& dl = renderInfo.scene->GetRegistry().get<DirectionalLight>(entity);
			const Transform& transform = renderInfo.scene->GetRegistry().get<Transform>(entity);

			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.color", dl.color);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.intensity", dl.intensity);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.ambient", dl.ambient);

			const glm::vec3 directionWorldSpace = transform.GetForward();
			const glm::vec3 directionViewSpace = glm::normalize(glm::mat3(camera.GetViewMat4()) * directionWorldSpace);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.directionWorldSpace", directionWorldSpace);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.directionViewSpace", directionViewSpace);

			const int hasDirectionalLight = 1;
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "hasDirectionalLight", hasDirectionalLight);

			const GraphicsSettings::Shadows::CSM& shadowSettings = renderInfo.scene->GetGraphicsSettings().shadows.csm;

			const int isEnabled = shadowSettings.isEnabled;
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.isEnabled", isEnabled);

			CSMRenderer* csmRenderer = (CSMRenderer*)renderInfo.renderView->GetCustomData("CSMRenderer");
			if (shadowSettings.isEnabled && !csmRenderer->GetLightSpaceMatrices().empty())
			{
				// Also in Shaders/Includes/CSM.h
				constexpr size_t maxCascadeCount = 10;

				std::vector<glm::vec4> shadowCascadeLevels(maxCascadeCount, glm::vec4{});
				for (size_t i = 0; i < csmRenderer->GetDistances().size(); i++)
				{
					shadowCascadeLevels[i] = glm::vec4(csmRenderer->GetDistances()[i]);
				}

				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.lightSpaceMatrices", *csmRenderer->GetLightSpaceMatrices().data());

				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.distances", *shadowCascadeLevels.data());

				const int cascadeCount = csmRenderer->GetLightSpaceMatrices().size();
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.cascadeCount", cascadeCount);

				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.fogFactor", shadowSettings.fogFactor);
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.maxDistance", shadowSettings.maxDistance);
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.pcfRange", shadowSettings.pcfRange);

				const int filter = (int)shadowSettings.filter;
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.filtering", filter);

				const int visualize = shadowSettings.visualize;
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.visualize", visualize);

				std::vector<glm::vec4> biases(maxCascadeCount);
				for (size_t i = 0; i < shadowSettings.biases.size(); i++)
				{
					biases[i] = glm::vec4(shadowSettings.biases[i]);
				}
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.biases", *biases.data());
			}
		}
		else
		{
			const int hasDirectionalLight = 0;
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "hasDirectionalLight", hasDirectionalLight);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.cascadeCount", hasDirectionalLight);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		const std::shared_ptr<Texture> colorTexture = renderInfo.renderView->GetFrameBuffer(Transparent)->GetAttachment(0);
		const std::shared_ptr<Texture> emissiveTexture = renderInfo.renderView->GetFrameBuffer(GBuffer)->GetAttachment(3);

		const std::shared_ptr<UniformWriter> outputUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, "DeferredOutput");
		outputUniformWriter->WriteTextureToFrame("outColor", colorTexture);
		outputUniformWriter->WriteTextureToFrame("outEmissive", emissiveTexture);
		
		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			glm::uvec2 groupCount = renderInfo.viewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

