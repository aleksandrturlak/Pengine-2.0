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
#include "../../Graphics/BarrierBatch.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/RenderView.h"
#include "../../Graphics/GraphicsPipeline.h"
#include "../../EventSystem/EventSystem.h"
#include "../../EventSystem/NextFrameEvent.h"

#include "../ViewportManager.h"
#include "../Viewport.h"

using namespace Pengine;

void RenderPassManager::CreateMotionVectorsPass()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = MotionVectors;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(MotionVectors);

		const GraphicsSettings& graphicsSettings = renderInfo.scene->GetGraphicsSettings();
		if (graphicsSettings.antialiasing.mode != GraphicsSettings::Antialiasing::Mode::TAA)
		{
			return;
		}

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "MotionVectors.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const glm::ivec2 viewportSize = renderInfo.viewportSize;

		Texture::CreateInfo texCreateInfo{};
		texCreateInfo.aspectMask = Texture::AspectMask::COLOR;
		texCreateInfo.instanceSize = 4;
		texCreateInfo.format = Format::R16G16_SFLOAT;
		texCreateInfo.size = viewportSize;
		texCreateInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		texCreateInfo.isMultiBuffered = false;
		texCreateInfo.filepath = passName;
		texCreateInfo.name = passName;

		std::shared_ptr<Texture> motionVectorsTexture = renderInfo.renderView->GetStorageImage(passName);
		if (!motionVectorsTexture || motionVectorsTexture->GetSize() != viewportSize)
		{
			motionVectorsTexture = Texture::Create(texCreateInfo);
			renderInfo.renderView->SetStorageImage(passName, motionVectorsTexture);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		renderUniformWriter->WriteTextureToFrame("outMotionVectors", motionVectorsTexture);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			glm::uvec2 groupCount = glm::uvec2(viewportSize) / glm::uvec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->PipelineBarrier(
				BarrierBatch{}
					.Stages(PipelineStage::ComputeShader, PipelineStage::ComputeShader)
					.Image(motionVectorsTexture, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
				renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateTAAPass()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = TAA;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(TAA);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "TAA.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const glm::ivec2 viewportSize = renderInfo.viewportSize;

		TAAData* taaData = (TAAData*)renderInfo.renderView->GetCustomData("TAAData");
		if (!taaData)
		{
			taaData = new TAAData();
			renderInfo.renderView->SetCustomData("TAAData", taaData);
		}

		Texture::CreateInfo texCreateInfo{};
		texCreateInfo.aspectMask = Texture::AspectMask::COLOR;
		texCreateInfo.instanceSize = 4;
		texCreateInfo.format = Format::B10G11R11_UFLOAT_PACK32;
		texCreateInfo.size = viewportSize;
		texCreateInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		texCreateInfo.isMultiBuffered = false;

		const std::string histKey0 = "TAAHistory0";
		const std::string histKey1 = "TAAHistory1";

		auto getOrCreateTexture = [&](const std::string& key) -> std::shared_ptr<Texture>
		{
			auto tex = renderInfo.renderView->GetStorageImage(key);
			if (!tex || tex->GetSize() != viewportSize)
			{
				texCreateInfo.filepath = key;
				texCreateInfo.name = key;
				tex = Texture::Create(texCreateInfo);
				renderInfo.renderView->SetStorageImage(key, tex);
			}
			return tex;
		};

		auto hist0 = getOrCreateTexture(histKey0);
		auto hist1 = getOrCreateTexture(histKey1);

		const uint32_t histIdx = taaData->frameIndex % 2;
		std::shared_ptr<Texture> historyTex = (histIdx == 0) ? hist0 : hist1;
		std::shared_ptr<Texture> outputTex  = (histIdx == 0) ? hist1 : hist0;

		// Register TAA output for AntiAliasingAndCompose to reference as "TAA"
		renderInfo.renderView->SetStorageImage(passName, outputTex);

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		renderUniformWriter->WriteTextureToFrame("historyTexture", historyTex);
		renderUniformWriter->WriteTextureToFrame("outColor", outputTex);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		const std::string taaBufferName = "TAABuffer";
		const std::shared_ptr<Buffer> taaBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			taaBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		const GraphicsSettings& graphicsSettings = renderInfo.scene->GetGraphicsSettings();
		const bool taaActive = (graphicsSettings.antialiasing.mode == GraphicsSettings::Antialiasing::Mode::TAA);
		if (!taaActive)
		{
			// Force passthrough: shader outputs currentColor with no history blend
			taaData->frameIndex = 0;
		}

		const int frameIndex = (int)taaData->frameIndex;
		baseMaterial->WriteToBuffer(taaBuffer, taaBufferName, "frameIndex", frameIndex);

		const GraphicsSettings::Antialiasing::TAA& taaSettings = graphicsSettings.antialiasing.taa;
		baseMaterial->WriteToBuffer(taaBuffer, taaBufferName, "varianceGamma",  taaSettings.varianceGamma);
		baseMaterial->WriteToBuffer(taaBuffer, taaBufferName, "minBlendFactor", taaSettings.minBlendFactor);
		baseMaterial->WriteToBuffer(taaBuffer, taaBufferName, "maxBlendFactor", taaSettings.maxBlendFactor);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			glm::uvec2 groupCount = glm::uvec2(viewportSize) / glm::uvec2(8, 8);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->PipelineBarrier(
				BarrierBatch{}
					.Stages(PipelineStage::ComputeShader, PipelineStage::FragmentShader)
					.Image(outputTex, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
				renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);

			taaData->frameIndex++;
		}
	};

	CreateComputePass(createInfo);
}
