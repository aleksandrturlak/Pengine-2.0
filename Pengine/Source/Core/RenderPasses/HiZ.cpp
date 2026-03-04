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

void RenderPassManager::CreateHiZPyramid()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = HiZPyramid;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(HiZPyramid);

		if (!renderInfo.scene->GetGraphicsSettings().hiZOcclusionCulling.isEnabled)
		{
			renderInfo.renderView->DeleteBuffer("HiZBuffer");
			renderInfo.renderView->DeleteBuffer("HiZAtomicBuffer");
			renderInfo.renderView->DeleteStorageImage(passName);
			renderInfo.renderView->DeleteUniformWriter(passName);

			return;
		}

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "HiZPyramid.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const glm::ivec2 viewportSize = renderInfo.viewportSize;
		const uint32_t mipLevelCount = static_cast<uint32_t>(std::floor(std::log2(std::max(
			viewportSize.x, viewportSize.y)))) + 1;

		Texture::CreateInfo textureCreateInfo{};
		textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
		textureCreateInfo.instanceSize = sizeof(float);
		textureCreateInfo.filepath = passName;
		textureCreateInfo.name = passName;
		textureCreateInfo.format = Format::R32_SFLOAT;
		textureCreateInfo.size = viewportSize;
		textureCreateInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::TRANSFER_DST };
		textureCreateInfo.isMultiBuffered = false;
		textureCreateInfo.mipLevels = mipLevelCount;

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		bool needToWriteToUniformWriter = false;
		std::shared_ptr<Texture> hiZTexture = renderInfo.renderView->GetStorageImage(passName);
		if (!hiZTexture)
		{
			hiZTexture = Texture::Create(textureCreateInfo);
			renderInfo.renderView->SetStorageImage(passName, hiZTexture);
			needToWriteToUniformWriter = true;
		}

		if (viewportSize != hiZTexture->GetSize())
		{
			hiZTexture = Texture::Create(textureCreateInfo);
			renderInfo.renderView->SetStorageImage(passName, hiZTexture);
		}

		if (needToWriteToUniformWriter)
		{
			std::vector<UniformWriter::TextureInfo> textureInfos(mipLevelCount);
			for (size_t i = 0; i < mipLevelCount; i++)
			{
				UniformWriter::TextureInfo& textureInfo = textureInfos[i];
				textureInfo.texture = hiZTexture;
				textureInfo.baseMipLevel = i;
			}
			
			renderUniformWriter->WriteTexturesToAllFrames("outHiZPyramid", textureInfos, 0);
		}

		const std::shared_ptr<Buffer> hiZBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			"HiZBuffer",
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		const std::shared_ptr<Buffer> hiZAtomicBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			"HiZAtomicBuffer",
			{},
			{ Buffer::Usage::STORAGE_BUFFER },
			MemoryType::GPU,
			true);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		uint32_t workgroupsX = (renderInfo.viewportSize.x + 64 - 1) / 64;
    	uint32_t workgroupsY = (renderInfo.viewportSize.y + 64 - 1) / 64;
		uint32_t workgroupCount = workgroupsX * workgroupsY;
		
		baseMaterial->WriteToBuffer(hiZBuffer, "HiZBuffer", "sourceSize", renderInfo.viewportSize);
		baseMaterial->WriteToBuffer(hiZBuffer, "HiZBuffer", "mipLevelCount", mipLevelCount);
		baseMaterial->WriteToBuffer(hiZBuffer, "HiZBuffer", "workgroupCount", workgroupCount);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);
			
			renderInfo.renderer->FillBuffer(hiZAtomicBuffer->GetNativeHandle(), hiZAtomicBuffer->GetSize(), 0, 0, renderInfo.frame);

			glm::uvec2 groupCount = viewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

