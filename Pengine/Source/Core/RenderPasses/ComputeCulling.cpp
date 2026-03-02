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

void RenderPassManager::CreateComputeIndirectDrawGBuffer()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = ComputeIndirectDrawGBuffer;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(ComputeIndirectDrawGBuffer);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "ComputeIndirectDrawGBuffer.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		MultiPassEntityData* multiPassData = (MultiPassEntityData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassEntityData");
		if (!multiPassData || multiPassData->passesByName.find(GBuffer) == multiPassData->passesByName.end())
		{
			return;
		}

		if (multiPassData->totalEntityCount == 0)
		{
			return;
		}

		const GraphicsSettings::HiZOcclusionCulling& hiZOcclusionCulling = renderInfo.scene->GetGraphicsSettings().hiZOcclusionCulling;

		const auto& gbufferData = multiPassData->passesByName[GBuffer];

		const std::shared_ptr<UniformWriter>& renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, ComputeIndirectDrawGBuffer, {}, true);
		const std::shared_ptr<Buffer>& indirectDrawCommandsBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "IndirectDrawCommands", {}, { Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::GPU, true);
		const std::shared_ptr<Buffer>& indirectDrawCommandCountBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "IndirectDrawCommandCount", {}, { Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::GPU, true);
		const std::shared_ptr<Buffer>& pipelineInfoBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "PipelineInfoBuffer", {}, { Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::CPU, true);
		const std::shared_ptr<Buffer>& hiZPyramidMipLevelCountBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "HiZPyramidMipLevelCountBuffer", {}, { Buffer::Usage::UNIFORM_BUFFER }, MemoryType::CPU, true);

		uint32_t mipLevelCount = 1;
		const std::shared_ptr<Texture> hiZTexture = renderInfo.renderView->GetStorageImage(HiZPyramid);
		if (hiZTexture)
		{
			mipLevelCount = hiZTexture->GetMipLevels();
			std::vector<UniformWriter::TextureInfo> textureInfos(mipLevelCount);
			for (size_t i = 0; i < mipLevelCount; i++)
			{
				UniformWriter::TextureInfo& textureInfo = textureInfos[i];
				textureInfo.texture = hiZTexture;
				textureInfo.baseMipLevel = i;
			}
			
			renderUniformWriter->WriteTexturesToFrame(
				"hiZPyramid",
				textureInfos,
				0,
				(Vk::frameInFlightIndex + 1) % Vk::frameInFlightCount,
				Vk::frameInFlightIndex);
		}
		else
		{
			renderUniformWriter->DeleteTexture("hiZPyramid");
		}

		const int isHiZOcclusionCullingEnabled = hiZOcclusionCulling.isEnabled && hiZTexture;
		baseMaterial->WriteToBuffer(hiZPyramidMipLevelCountBuffer, "HiZPyramidMipLevelCountBuffer", "isHiZOcclusionCullingEnabled", isHiZOcclusionCullingEnabled);
		baseMaterial->WriteToBuffer(hiZPyramidMipLevelCountBuffer, "HiZPyramidMipLevelCountBuffer", "depthBias", hiZOcclusionCulling.depthBias);
		baseMaterial->WriteToBuffer(hiZPyramidMipLevelCountBuffer, "HiZPyramidMipLevelCountBuffer", "mipLevelCount", mipLevelCount);

		uint32_t offset = 0;
		for (const auto& [id, pipeline] : gbufferData.sortedPipelines)
		{
			pipelineInfoBuffer->WriteToBuffer(
				(void*)&offset,
				sizeof(uint32_t),
				sizeof(uint32_t) * id);
			offset += gbufferData.pipelineInfos.at(pipeline).maxDrawCount;
		}
		pipelineInfoBuffer->Flush();

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName,  topLevelRenderPassDebugColor, renderInfo.frame);

			renderInfo.renderer->FillBuffer(
				indirectDrawCommandCountBuffer->GetNativeHandle(),
				indirectDrawCommandCountBuffer->GetSize(),
				0,
				0,
				renderInfo.frame);

			uint32_t groupCount = (multiPassData->totalEntityCount + 15) / 16;
			renderInfo.renderer->Compute(pipeline, { groupCount, 1, 1 }, uniformWriterNativeHandles, renderInfo.frame);
			
			renderInfo.renderer->PipelineBarrier(
				BarrierBatch{}
					.Stages(PipelineStage::ComputeShader, PipelineStage::DrawIndirect)
					.Buffer(indirectDrawCommandsBuffer->GetNativeHandle(), Access::ShaderWrite, Access::IndirectCommandRead)
					.Buffer(indirectDrawCommandCountBuffer->GetNativeHandle(), Access::ShaderWrite, Access::IndirectCommandRead),
				renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateComputeIndirectDrawCSM()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = ComputeIndirectDrawCSM;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(ComputeIndirectDrawCSM);

		const GraphicsSettings::Shadows::CSM& shadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.csm;
		if (!shadowsSettings.isEnabled || renderInfo.scene->GetGraphicsSettings().rayTracing.shadows.directionalLight)
		{
			return;
		}

		auto directionalLightView = renderInfo.scene->GetRegistry().view<DirectionalLight>();
		if (directionalLightView.empty())
		{
			return;
		}

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "ComputeIndirectDrawCSM.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		MultiPassEntityData* multiPassData = (MultiPassEntityData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassEntityData");
		if (!multiPassData || multiPassData->passesByName.find(CSM) == multiPassData->passesByName.end())
		{
			return;
		}

		if (multiPassData->totalEntityCount == 0)
		{
			return;
		}

		const auto& csmData = multiPassData->passesByName[CSM];
		if (csmData.entityCount == 0)
		{
			return;
		}

		CSMRenderer* csmRenderer = (CSMRenderer*)renderInfo.renderView->GetCustomData("CSMRenderer");
		if (!csmRenderer)
		{
			csmRenderer = new CSMRenderer();
			renderInfo.renderView->SetCustomData("CSMRenderer", csmRenderer);
		}

		glm::vec3 lightDirection{};
		{
			const entt::entity& entity = directionalLightView.back();
			const Transform& transform = renderInfo.scene->GetRegistry().get<Transform>(entity);
			lightDirection = transform.GetForward();
		}

		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		const Transform& cameraTransform = renderInfo.camera->GetComponent<Transform>();

		const glm::ivec2 resolutions[3] = { { 1024, 1024 }, { 2048, 2048 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapSize = resolutions[shadowsSettings.quality];

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

		const int cascadeCount = csmRenderer->GetLightSpaceMatrices().size();

		if (cascadeCount == 0)
		{
			return;
		}

		{
			const Camera& camera = renderInfo.camera->GetComponent<Camera>();
			const GraphicsSettings& graphicsSettings = renderInfo.scene->GetGraphicsSettings();
			entt::registry& registry = renderInfo.scene->GetRegistry();

			const std::shared_ptr<BaseMaterial> reflectionBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
				std::filesystem::path("Materials") / "DefaultReflection.basemat");
			const std::shared_ptr<Pipeline> reflectionPipeline = reflectionBaseMaterial->GetPipeline(DefaultReflection);
			if (!reflectionPipeline)
			{
				FATAL_ERROR("DefaultReflection base material is broken! No pipeline found!");
			}

			const std::string csmBufferName = "CSMBuffer";
			const std::shared_ptr<UniformWriter> lightsUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, reflectionPipeline, Pipeline::DescriptorSetIndexType::RENDERER, "Camera");
			const std::shared_ptr<Buffer> csmBuffer = GetOrCreateBuffer(
				renderInfo.renderView,
				lightsUniformWriter,
				csmBufferName,
				{},
				{ Buffer::Usage::UNIFORM_BUFFER },
				MemoryType::CPU,
				true);

			auto directionalLightView = registry.view<DirectionalLight>();
			if (!directionalLightView.empty())
			{
				const entt::entity& entity = directionalLightView.back();
				DirectionalLight& dl = registry.get<DirectionalLight>(entity);
				const Transform& transform = registry.get<Transform>(entity);

				const GraphicsSettings::Shadows::CSM& shadowSettings = graphicsSettings.shadows.csm;
				const int isEnabled = shadowSettings.isEnabled;
				reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.isEnabled", isEnabled);

				CSMRenderer* csmRenderer = (CSMRenderer*)renderInfo.renderView->GetCustomData("CSMRenderer");
				if (shadowSettings.isEnabled && csmRenderer && !csmRenderer->GetLightSpaceMatrices().empty())
				{
					constexpr size_t maxCascadeCount = 10;

					std::vector<glm::vec4> shadowCascadeLevels(maxCascadeCount, glm::vec4{});
					for (size_t i = 0; i < csmRenderer->GetDistances().size(); i++)
					{
						shadowCascadeLevels[i] = glm::vec4(csmRenderer->GetDistances()[i]);
					}

					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.lightSpaceMatrices", *csmRenderer->GetLightSpaceMatrices().data());
					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.distances", *shadowCascadeLevels.data());

					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.cascadeCount", cascadeCount);
					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.fogFactor", shadowSettings.fogFactor);
					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.maxDistance", shadowSettings.maxDistance);
					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.pcfRange", shadowSettings.pcfRange);

					const int filter = (int)shadowSettings.filter;
					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.filtering", filter);

					const int visualize = shadowSettings.visualize;
					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.visualize", visualize);

					std::vector<glm::vec4> biases(maxCascadeCount);
					for (size_t i = 0; i < shadowSettings.biases.size(); i++)
					{
						biases[i] = glm::vec4(shadowSettings.biases[i]);
					}
					reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.biases", *biases.data());
				}
			}
			else
			{
				const int hasDirectionalLight = 0;
				reflectionBaseMaterial->WriteToBuffer(csmBuffer, csmBufferName, "csm.cascadeCount", hasDirectionalLight);
			}
		}

		const std::shared_ptr<UniformWriter>& renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, "ComputeIndirectDrawCSMBuffers", {}, true);
		const std::shared_ptr<Buffer>& indirectDrawCommandsBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "IndirectDrawCommands", "IndirectDrawCommandsCSM", 
			{ Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::GPU, true);
		const std::shared_ptr<Buffer>& indirectDrawCommandCountBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "IndirectDrawCommandCount", "IndirectDrawCommandCountCSM", 
			{ Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::GPU, true);
		const std::shared_ptr<Buffer>& pipelineInfoBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "PipelineInfoBuffer", "PipelineInfoBufferCSM", 
			{ Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::CPU, true);
		const std::shared_ptr<Buffer>& csmInstanceDataBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "CSMInstanceDataBuffer", "CSMInstanceDataBufferCSM", 
			{ Buffer::Usage::STORAGE_BUFFER }, MemoryType::GPU, true);

		uint32_t pipelineOffset = 0;
		for (int cascade = 0; cascade < cascadeCount; ++cascade)
		{
			for (const auto& [id, pipelinePtr] : csmData.sortedPipelines)
			{
				uint32_t cascadePipelineIndex = cascade * MAX_PIPELINE_COUNT + id;
				pipelineInfoBuffer->WriteToBuffer(
					(void*)&pipelineOffset,
					sizeof(uint32_t),
					sizeof(uint32_t) * cascadePipelineIndex);
				pipelineOffset += csmData.pipelineInfos.at(pipelinePtr).maxDrawCount;
			}
		}
		pipelineInfoBuffer->Flush();

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			renderInfo.renderer->FillBuffer(
				indirectDrawCommandCountBuffer->GetNativeHandle(),
				indirectDrawCommandCountBuffer->GetSize(),
				0,
				0,
				renderInfo.frame);

			uint32_t groupCount = (multiPassData->totalEntityCount + 15) / 16;
			renderInfo.renderer->Compute(pipeline, { groupCount, 1, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->PipelineBarrier(
				BarrierBatch{}
					.Stages(PipelineStage::ComputeShader, PipelineStage::DrawIndirect)
					.Buffer(indirectDrawCommandsBuffer->GetNativeHandle(), Access::ShaderWrite, Access::IndirectCommandRead)
					.Buffer(indirectDrawCommandCountBuffer->GetNativeHandle(), Access::ShaderWrite, Access::IndirectCommandRead),
				renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}
