#include "../RenderPassManager.h"

#include "../Logger.h"
#include "../BindlessUniformWriter.h"
#include "../DDGIRenderer.h"
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

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		const std::shared_ptr<Texture> colorTexture = renderInfo.renderView->GetFrameBuffer(Transparent)->GetAttachment(0);
		const std::shared_ptr<Texture> emissiveTexture = renderInfo.renderView->GetFrameBuffer(GBuffer)->GetAttachment(3);

		renderUniformWriter->WriteTextureToFrame("outColor", colorTexture);
		renderUniformWriter->WriteTextureToFrame("outEmissive", emissiveTexture);

		if (renderInfo.scene->GetTLAS())
		{
			renderUniformWriter->WriteAccelerationStructureToFrame("topLevelAS", renderInfo.scene->GetTLAS());
		}

		// DDGI — bind probe atlases and upload grid data UBO
		{
			DDGIRenderer* ddgi = (DDGIRenderer*)renderInfo.renderView->GetCustomData("DDGIRenderer");
			DDGIRenderer::DDGIData ddgiData{};
			if (ddgi && ddgi->m_IsEnabled && ddgi->m_IrradianceAtlas && ddgi->m_VisibilityAtlas)
			{
				const glm::vec3 cameraPos = renderInfo.camera
					? renderInfo.camera->GetComponent<Transform>().GetPosition()
					: glm::vec3(0.0f);
				ddgiData = ddgi->BuildShaderData(cameraPos);
				renderUniformWriter->WriteTextureToFrame("ddgiIrradianceAtlas", ddgi->m_IrradianceAtlas);
				renderUniformWriter->WriteTextureToFrame("ddgiVisibilityAtlas",  ddgi->m_VisibilityAtlas);
			}

			const std::shared_ptr<Buffer> ddgiBuffer = GetOrCreateBuffer(
				renderInfo.renderView,
				renderUniformWriter,
				"DDGIUniform",
				{},
				{ Buffer::Usage::UNIFORM_BUFFER },
				MemoryType::CPU,
				true);
			baseMaterial->WriteToBuffer(ddgiBuffer, "DDGIUniform", "ddgi", ddgiData);

			renderUniformWriter->WriteBufferToFrame("DDGIUniform", ddgiBuffer);
		}

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

