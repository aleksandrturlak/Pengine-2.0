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

void RenderPassManager::CreateCSM()
{
	RenderPass::ClearDepth clearDepth{};
	clearDepth.clearDepth = 1.0f;
	clearDepth.clearStencil = 0;

	RenderPass::AttachmentDescription depth{};
	depth.textureCreateInfo.format = Format::D32_SFLOAT;
	depth.textureCreateInfo.aspectMask = Texture::AspectMask::DEPTH;
	depth.textureCreateInfo.instanceSize = sizeof(float);
	depth.textureCreateInfo.isMultiBuffered = true;
	depth.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::DEPTH_STENCIL_ATTACHMENT };
	depth.textureCreateInfo.name = "CSM";
	depth.textureCreateInfo.filepath = depth.textureCreateInfo.name;
	depth.layout = Texture::Layout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Texture::SamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.filter = Texture::SamplerCreateInfo::Filter::LINEAR;
	samplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_WHITE;
	samplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_EDGE;
	samplerCreateInfo.maxAnisotropy = 1.0f;

	depth.textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = CSM;
	createInfo.clearDepths = { clearDepth };
	createInfo.attachmentDescriptions = { depth };
	createInfo.resizeWithViewport = false;
	createInfo.createFrameBuffer = false;

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(CSM);
		const std::string& renderPassName = renderInfo.renderPass->GetName();

		const GraphicsSettings::Shadows::CSM& shadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.csm;
		if (!shadowsSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(renderPassName);
			renderInfo.renderView->DeleteCustomData("CSMRenderer");
			renderInfo.renderView->DeleteBuffer("LightSpaceMatrices");
			renderInfo.renderView->DeleteFrameBuffer(renderPassName);

			return;
		}

		CSMRenderer* csmRenderer = (CSMRenderer*)renderInfo.renderView->GetCustomData("CSMRenderer");
		if (!csmRenderer)
		{
			csmRenderer = new CSMRenderer();

			renderInfo.renderView->SetCustomData("CSMRenderer", csmRenderer);
		}

		const glm::ivec2 resolutions[3] = { { 1024, 1024 }, { 2048, 2048 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapSize = resolutions[shadowsSettings.quality];

		std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer(renderPassName);
		if (!frameBuffer)
		{
			const std::string& renderPassName = renderInfo.renderPass->GetName();
			renderInfo.renderPass->GetAttachmentDescriptions().back().textureCreateInfo.layerCount = shadowsSettings.cascadeCount;
			frameBuffer = FrameBuffer::Create(renderInfo.renderPass, renderInfo.renderView.get(), shadowMapSize);

			renderInfo.renderView->SetFrameBuffer(renderPassName, frameBuffer);
		}

		const uint32_t currentLayerCount = frameBuffer->GetAttachmentCreateInfos().back().layerCount;
		const bool needsLayerCountUpdate = currentLayerCount != static_cast<uint32_t>(shadowsSettings.cascadeCount);
		
		if (frameBuffer->GetSize() != shadowMapSize || needsLayerCountUpdate)
		{
			frameBuffer->GetAttachmentCreateInfos().back().layerCount = shadowsSettings.cascadeCount;
			frameBuffer->Resize(shadowMapSize);
		}

		const std::shared_ptr<Scene> scene = renderInfo.scene;
		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		entt::registry& registry = scene->GetRegistry();

		glm::vec3 lightDirection{};
		auto directionalLightView = renderInfo.scene->GetRegistry().view<DirectionalLight>();
		if (directionalLightView.empty())
		{
			return;
		}

		{
			const entt::entity& entity = directionalLightView.back();
			DirectionalLight& dl = renderInfo.scene->GetRegistry().get<DirectionalLight>(entity);
			const Transform& transform = renderInfo.scene->GetRegistry().get<Transform>(entity);
			lightDirection = transform.GetForward();
		}

		const glm::mat4 projectionMat4 = glm::perspective(
			camera.GetFov(),
			(float)renderInfo.viewportSize.x / (float)renderInfo.viewportSize.y,
			camera.GetZNear(),
			shadowsSettings.maxDistance);

		csmRenderer->GenerateLightSpaceMatrices(
			projectionMat4 * camera.GetViewMat4(),
			lightDirection,
			shadowMapSize,
			camera.GetZNear(),
			shadowsSettings.maxDistance,
			shadowsSettings.cascadeCount,
			shadowsSettings.splitFactor,
			shadowsSettings.stabilizeCascades);

		MultiPassEntityData* multiPassData = (MultiPassEntityData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassEntityData");

		RenderPass::SubmitInfo submitInfo{};
		submitInfo.frame = renderInfo.frame;
		submitInfo.renderPass = renderInfo.renderPass;
		submitInfo.frameBuffer = frameBuffer;

		const int cascadeCount = static_cast<int>(csmRenderer->GetLightSpaceMatrices().size());

		if (multiPassData && multiPassData->passesByName.find(CSM) != multiPassData->passesByName.end())
		{
			const auto& csmData = multiPassData->passesByName[CSM];

			const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
				std::filesystem::path("Materials") / "MeshBase.basemat");
			if (!baseMaterial)
			{
				renderInfo.renderer->BeginRenderPass(submitInfo);
				renderInfo.renderer->EndRenderPass(submitInfo);
				return;
			}

			std::shared_ptr<Buffer> lightSpaceMatricesBuffer;

			renderInfo.renderer->BeginRenderPass(submitInfo);

			const std::shared_ptr<Buffer>& csmInstanceDataBuffer = GetOrCreateBuffer(
				renderInfo.renderView, nullptr, "CSMInstanceDataBufferCSM");
			const std::shared_ptr<Buffer>& indirectDrawCommandsBuffer = GetOrCreateBuffer(
				renderInfo.renderView, nullptr, "IndirectDrawCommandsCSM");
			const std::shared_ptr<Buffer>& indirectDrawCommandCountBuffer = GetOrCreateBuffer(
				renderInfo.renderView, nullptr, "IndirectDrawCommandCountCSM");
			const std::shared_ptr<Buffer>& pipelineInfoBuffer = GetOrCreateBuffer(
				renderInfo.renderView, nullptr, "PipelineInfoBufferCSM");

			for (int cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
			{
				renderInfo.renderer->BeginCommandLabel(std::format("Cascade {}", cascadeIndex), glm::vec3(1.0f, 1.0f, 0.0f), renderInfo.frame);
				
				for (const auto& [id, pipeline] : csmData.sortedPipelines)
				{
					renderInfo.renderer->BindPipeline(pipeline, renderInfo.frame);

					const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
						renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, renderPassName);
					const std::string lightSpaceMatricesBufferName = "LightSpaceMatrices";
					lightSpaceMatricesBuffer = GetOrCreateBuffer(
						renderInfo.renderView,
						renderUniformWriter,
						lightSpaceMatricesBufferName,
						{},
						{ Buffer::Usage::UNIFORM_BUFFER },
						MemoryType::CPU,
						true);

					if (cascadeIndex == 0)
					{
						baseMaterial->WriteToBuffer(
							lightSpaceMatricesBuffer,
							lightSpaceMatricesBufferName,
							"lightSpaceMatrices",
							*csmRenderer->GetLightSpaceMatrices().data());

						baseMaterial->WriteToBuffer(
							lightSpaceMatricesBuffer,
							lightSpaceMatricesBufferName,
							"cascadeCount",
							cascadeCount);
						lightSpaceMatricesBuffer->Flush();

						renderUniformWriter->WriteBufferToFrame("CSMInstanceDataBuffer", csmInstanceDataBuffer);
					}

					if (BindAndFlushUniformWriters(
						pipeline,
						nullptr,
						nullptr,
						renderInfo,
						{
							Pipeline::DescriptorSetIndexType::BINDLESS,
							Pipeline::DescriptorSetIndexType::SCENE,
							Pipeline::DescriptorSetIndexType::BASE_MATERIAL,
							Pipeline::DescriptorSetIndexType::RENDERER,
							Pipeline::DescriptorSetIndexType::RENDERPASS
						}
					))
					{
						uint32_t cascadePipelineIndex = cascadeIndex * MAX_PIPELINE_COUNT + id;
						uint32_t commandOffset = reinterpret_cast<uint32_t*>(pipelineInfoBuffer->GetData())[cascadePipelineIndex];

						renderInfo.renderer->DrawIndirectCount(
							indirectDrawCommandsBuffer->GetNativeHandle(),
							commandOffset,
							indirectDrawCommandCountBuffer->GetNativeHandle(),
							cascadePipelineIndex,
							csmData.pipelineInfos.at(pipeline).maxDrawCount,
							renderInfo.frame);
					}
				}

				renderInfo.renderer->EndCommandLabel(renderInfo.frame);
			}

			renderInfo.renderer->EndRenderPass(submitInfo);
		}
		else
		{
			renderInfo.renderer->BeginRenderPass(submitInfo);
			renderInfo.renderer->EndRenderPass(submitInfo);
		}
	};

	CreateRenderPass(createInfo);
}

