#include "../RenderPassManager.h"

#include "../GraphicsSettings.h"
#include "../Logger.h"
#include "../Scene.h"
#include "../MaterialManager.h"
#include "../TextureManager.h"
#include "../Profiler.h"

#include "../../Graphics/Renderer.h"
#include "../../Graphics/RenderView.h"
#include "../../Graphics/BarrierBatch.h"

using namespace Pengine;

void RenderPassManager::CreateBloom()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = Bloom;

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(Bloom);

		GraphicsSettings::Bloom& bloomSettings = renderInfo.scene->GetGraphicsSettings().bloom;
		
		if (!bloomSettings.isEnabled)
		{
			renderInfo.renderView->DeleteCustomData("BloomData");
			renderInfo.renderView->DeleteStorageImage(Bloom);
			renderInfo.renderView->DeleteUniformWriter("BloomDown");
			renderInfo.renderView->DeleteUniformWriter("BloomUp");
			return;
		}
		
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 viewportSize = glm::vec2(renderInfo.viewportSize) * resolutionScales[bloomSettings.resolutionScale];
		const int maxMipCount = static_cast<int>(std::floor(std::log2(std::max(
			viewportSize.x, viewportSize.y)))) + 1;
		int mipCount = bloomSettings.mipCount = std::min(bloomSettings.mipCount, maxMipCount);

		struct BloomData : public CustomData
		{
			int mipCount = 0;
			float resolutionScale = 0;
		};

		BloomData* bloomData = (BloomData*)renderInfo.renderView->GetCustomData("BloomData");
		if (!bloomData)
		{
			bloomData = new BloomData();
			renderInfo.renderView->SetCustomData("BloomData", bloomData);
		}

		std::shared_ptr<Texture> bloomTexture = renderInfo.renderView->GetStorageImage(Bloom);

		auto createBloomTexture = [&]()
		{
			Texture::CreateInfo textureCreateInfo{};
			textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
			textureCreateInfo.instanceSize = sizeof(uint32_t);
			textureCreateInfo.format = Format::B10G11R11_UFLOAT_PACK32;
			textureCreateInfo.size = viewportSize;
			textureCreateInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_DST };
			textureCreateInfo.isMultiBuffered = false;
			textureCreateInfo.mipLevels = mipCount;
			textureCreateInfo.name = Bloom;
			textureCreateInfo.filepath = Bloom;
			textureCreateInfo.frame = renderInfo.frame;

			Texture::SamplerCreateInfo samplerCreateInfo{};
			samplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER;
			samplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_BLACK;
			samplerCreateInfo.maxAnisotropy = 1.0f;

    		textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

			bloomTexture = Texture::Create(textureCreateInfo);
			renderInfo.renderView->SetStorageImage(Bloom, bloomTexture);
		};

		bool recreateResources = false;
		if (!bloomTexture ||
			bloomData->resolutionScale != resolutionScales[bloomSettings.resolutionScale] ||
			bloomData->mipCount != mipCount)
		{
			createBloomTexture();
			recreateResources = true;
		}

		bloomData->resolutionScale = resolutionScales[bloomSettings.resolutionScale];
		bloomData->mipCount = mipCount;

		const std::shared_ptr<BaseMaterial> downBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "BloomDownSample.basemat");
		const std::shared_ptr<Pipeline> downPipeline = downBaseMaterial->GetPipeline(Bloom);
		if (!downPipeline)
			return;

		const std::shared_ptr<BaseMaterial> upBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "BloomUpSample.basemat");
		const std::shared_ptr<Pipeline> upPipeline = upBaseMaterial->GetPipeline(Bloom);
		if (!upPipeline)
			return;

		const std::shared_ptr<UniformWriter> downUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, downPipeline, Pipeline::DescriptorSetIndexType::RENDERER, "BloomDown");

		const std::shared_ptr<UniformWriter> upUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, upPipeline, Pipeline::DescriptorSetIndexType::RENDERER, "BloomUp");

		if (recreateResources)
		{
			std::vector<UniformWriter::TextureInfo> mipInfos(mipCount);
			for (int i = 0; i < mipCount; i++)
			{
				mipInfos[i].texture = bloomTexture;
				mipInfos[i].baseMipLevel = i;
			}
			downUniformWriter->WriteTexturesToAllFrames("outBloom", mipInfos);
			upUniformWriter->WriteTexturesToAllFrames("outBloom", mipInfos);
		}

		renderInfo.renderer->BeginCommandLabel(Bloom, topLevelRenderPassDebugColor, renderInfo.frame);

		renderInfo.renderer->ClearColorImage(bloomTexture, { 0.0f, 0.0f, 0.0f, 0.0f }, renderInfo.frame);

		const std::shared_ptr<Texture> blackTexture = TextureManager::GetInstance().GetBlack();

		// Down Sample: GBuffer emissive → bloom mip 0 → bloom mip 1 → ...
		{
			PROFILER_SCOPE("DownSample");

			std::shared_ptr<Texture> sourceTexture = renderInfo.renderView->GetFrameBuffer(GBuffer)->GetAttachment(3);

			renderInfo.renderer->BindPipeline(downPipeline, renderInfo.frame);

			glm::ivec2 mipSize = viewportSize;
			std::vector<UniformWriter::TextureInfo> sourceTextureInfos(mipCount, { blackTexture, 0 });

			for (int mipLevel = 1; mipLevel < mipCount; mipLevel++)
			{
				sourceTextureInfos[mipLevel - 1].baseMipLevel = (mipLevel - 1) > 0 ? mipLevel - 1 : 0;
				sourceTextureInfos[mipLevel - 1].texture = sourceTexture;

				renderInfo.renderer->BeginCommandLabel(std::format("DownSample[{}]", mipLevel), { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

				renderInfo.renderer->PipelineBarrier(
					BarrierBatch{}
						.Stages(PipelineStage::ComputeShader, PipelineStage::ComputeShader)
						.Image(bloomTexture, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
					renderInfo.frame);

				renderInfo.renderer->BindUniformWriters(
					downPipeline,
					{ downUniformWriter->GetNativeHandle() },
					0,
					renderInfo.frame);

				struct DownPushConstants
				{
					glm::vec2 sourceSize;
					int mipLevel;
					float bloomIntensity;
				} pc{};
				pc.sourceSize = glm::vec2(mipSize);
				pc.mipLevel = mipLevel;
				pc.bloomIntensity = bloomSettings.intensity;

				renderInfo.renderer->PushConstants(
					downPipeline,
					ShaderStage::Compute,
					0,
					sizeof(DownPushConstants),
					&pc,
					renderInfo.frame);

				const glm::ivec2 outputMipSize = glm::max(mipSize / 2, glm::ivec2(4));
				glm::uvec2 groupCount = outputMipSize / glm::ivec2(16, 16);
				groupCount += glm::uvec2(1, 1);
				renderInfo.renderer->Dispatch({ groupCount.x, groupCount.y, 1 }, renderInfo.frame);

				renderInfo.renderer->EndCommandLabel(renderInfo.frame);

				sourceTexture = bloomTexture;
				mipSize = outputMipSize;
			}

			downUniformWriter->WriteTexturesToFrame("sourceTexture", sourceTextureInfos);
			downUniformWriter->Flush();
		}

		// Up Sample: bloom mip N → mip N-1 (additive blend).
		{
			PROFILER_SCOPE("UpSample");
			
			std::vector<UniformWriter::TextureInfo> sourceTextureInfos(mipCount, { blackTexture, 0 });
			
			renderInfo.renderer->BindPipeline(upPipeline, renderInfo.frame);

			for (int mipLevel = mipCount - 1; mipLevel > 0; mipLevel--)
			{
				sourceTextureInfos[mipLevel].baseMipLevel = mipLevel;
				sourceTextureInfos[mipLevel].texture = bloomTexture;

				renderInfo.renderer->BeginCommandLabel(std::format("UpSample[{}]", mipLevel - 1), { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

				renderInfo.renderer->PipelineBarrier(
					BarrierBatch{}
						.Stages(PipelineStage::ComputeShader, PipelineStage::ComputeShader)
						.Image(bloomTexture, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
					renderInfo.frame);

				renderInfo.renderer->BindUniformWriters(
					upPipeline,
					{ upUniformWriter->GetNativeHandle() },
					0,
					renderInfo.frame);

				const int dstMipLevel = mipLevel - 1;

				struct UpPushConstants
				{
					int dstMipLevel;
				} pc{};
				pc.dstMipLevel = dstMipLevel;

				renderInfo.renderer->PushConstants(
					upPipeline,
					ShaderStage::Compute,
					0,
					sizeof(UpPushConstants),
					&pc,
					renderInfo.frame);

				const glm::ivec2 dstMipSize = glm::max(viewportSize / (1 << dstMipLevel), glm::ivec2(4));
				glm::uvec2 groupCount = dstMipSize / glm::ivec2(16, 16);
				groupCount += glm::uvec2(1, 1);
				renderInfo.renderer->Dispatch({ groupCount.x, groupCount.y, 1 }, renderInfo.frame);
				
				renderInfo.renderer->EndCommandLabel(renderInfo.frame);
			}

			upUniformWriter->WriteTexturesToFrame("sourceTexture", sourceTextureInfos);
			upUniformWriter->Flush();
		}

		renderInfo.renderer->PipelineBarrier(
			BarrierBatch{}
				.Stages(PipelineStage::ComputeShader, PipelineStage::FragmentShader | PipelineStage::ComputeShader)
				.Image(bloomTexture, ImageLayout::General, ImageLayout::General, Access::ShaderWrite, Access::ShaderRead),
			renderInfo.frame);

		renderInfo.renderer->EndCommandLabel(renderInfo.frame);
	};

	CreateComputePass(createInfo);
}
