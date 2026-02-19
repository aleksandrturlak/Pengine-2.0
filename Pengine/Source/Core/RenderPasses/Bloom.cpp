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

void RenderPassManager::CreateBloom()
{
	glm::vec4 clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	RenderPass::AttachmentDescription color{};
	color.textureCreateInfo.format = Format::B10G11R11_UFLOAT_PACK32;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint32_t);
	color.textureCreateInfo.isMultiBuffered = true;
	color.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::COLOR_ATTACHMENT };
	color.textureCreateInfo.name = "BloomColor";
	color.textureCreateInfo.filepath = color.textureCreateInfo.name;
	color.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	color.load = RenderPass::Load::LOAD;
	color.store = RenderPass::Store::STORE;

	Texture::SamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER;
	samplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_BLACK;
	samplerCreateInfo.maxAnisotropy = 1.0f;

	color.textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = Bloom;
	createInfo.clearColors = { clearColor };
	createInfo.attachmentDescriptions = { color };
	createInfo.resizeWithViewport = false;
	createInfo.createFrameBuffer = false;
	
	const std::shared_ptr<Mesh> planeMesh = nullptr;

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(Bloom);

		const std::string& renderPassName = renderInfo.renderPass->GetName();
		const GraphicsSettings::Bloom& bloomSettings = renderInfo.scene->GetGraphicsSettings().bloom;
		const int mipCount = bloomSettings.mipCount;

		if (!bloomSettings.isEnabled)
		{
			renderInfo.renderView->DeleteCustomData("BloomData");
			renderInfo.renderView->DeleteFrameBuffer(renderPassName);
			for (int mipLevel = 0; mipLevel < mipCount; mipLevel++)
			{
				renderInfo.renderView->DeleteFrameBuffer("BloomFrameBuffers(" + std::to_string(mipLevel) + ")");
				renderInfo.renderView->DeleteBuffer("BloomBuffers(" + std::to_string(mipLevel) + ")");
				renderInfo.renderView->DeleteUniformWriter("BloomDownUniformWriters(" + std::to_string(mipLevel) + ")");
				renderInfo.renderView->DeleteUniformWriter("BloomUpUniformWriters(" + std::to_string(mipLevel) + ")");
			}

			return;
		}

		const std::shared_ptr<Mesh> plane = MeshManager::GetInstance().LoadMesh("FullScreenQuad");

		// Used to store unique view (camera) bloom information to compare with current graphics settings
		// and in case if not equal then recreate resources.
		struct BloomData : public CustomData
		{
			int mipCount = 0;
			glm::ivec2 sourceSize = { 0, 0 };
		};
		
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		if (renderInfo.renderPass->GetResizeViewportScale() != glm::vec2(resolutionScales[bloomSettings.resolutionScale]))
		{
			renderInfo.renderPass->SetResizeViewportScale(glm::vec2(resolutionScales[bloomSettings.resolutionScale]));
		}
		const glm::ivec2 viewportSize = glm::vec2(renderInfo.viewportSize) * renderInfo.renderPass->GetResizeViewportScale();

		bool recreateResources = false;
		BloomData* bloomData = (BloomData*)renderInfo.renderView->GetCustomData("BloomData");
		if (!bloomData)
		{
			bloomData = new BloomData();
			bloomData->mipCount = mipCount;
			bloomData->sourceSize = viewportSize;
			renderInfo.renderView->SetCustomData("BloomData", bloomData);
		
			recreateResources = true;
		}
		else
		{
			recreateResources = bloomData->mipCount != mipCount;
			bloomData->mipCount = mipCount;
		}

		renderInfo.renderer->BeginCommandLabel("Bloom", topLevelRenderPassDebugColor, renderInfo.frame);
		// Down Sample.
		{
			PROFILER_SCOPE("DownSample");

			const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
				std::filesystem::path("Materials") / "BloomDownSample.basemat");
			const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(renderPassName);
			if (!pipeline)
			{
				return;
			}

			// Create FrameBuffers.
			if (recreateResources || !renderInfo.renderView->GetFrameBuffer(renderPassName))
			{
				glm::ivec2 size = viewportSize;
				for (int mipLevel = 0; mipLevel < mipCount; mipLevel++)
				{
					const std::string mipLevelString = std::to_string(mipLevel);

					// NOTE: There are parentheses in the name because square brackets are used in the base material file to find the attachment index.
					renderInfo.renderView->SetFrameBuffer("BloomFrameBuffers(" + mipLevelString + ")", FrameBuffer::Create(renderInfo.renderPass, renderInfo.renderView.get(), size));

					size.x = glm::max(size.x / 2, 4);
					size.y = glm::max(size.y / 2, 4);
				}

				// For viewport visualization and easy access by render pass name.
				renderInfo.renderView->SetFrameBuffer(renderPassName, renderInfo.renderView->GetFrameBuffer("BloomFrameBuffers(0)"));
			}

			// Create UniformWriters, Buffers for down sample pass.
			if (recreateResources)
			{
				for (int mipLevel = 0; mipLevel < mipCount; mipLevel++)
				{
					const std::string mipLevelString = std::to_string(mipLevel);

					const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
						renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, renderPassName, "BloomDownUniformWriters[" + mipLevelString + "]");
					GetOrCreateBuffer(
						renderInfo.renderView,
						renderUniformWriter,
						"MipBuffer",
						"BloomBuffers[" + mipLevelString + "]",
						{ Buffer::Usage::UNIFORM_BUFFER },
						MemoryType::CPU,
						true);
				}
			}

			// Resize.
			if (bloomData->sourceSize.x != viewportSize.x ||
				bloomData->sourceSize.y != viewportSize.y)
			{
				glm::ivec2 size = viewportSize;
				bloomData->sourceSize = size;
				for (size_t mipLevel = 0; mipLevel < mipCount; mipLevel++)
				{
					const std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer("BloomFrameBuffers(" + std::to_string(mipLevel) + ")");
					frameBuffer->Resize(size);

					size.x = glm::max(size.x / 2, 1);
					size.y = glm::max(size.y / 2, 1);
				}
			}

			std::shared_ptr<Texture> sourceTexture = renderInfo.renderView->GetFrameBuffer(GBuffer)->GetAttachment(3);
			for (int mipLevel = 0; mipLevel < mipCount; mipLevel++)
			{
				const std::string mipLevelString = std::to_string(mipLevel);

				renderInfo.renderer->MemoryBarrierFragmentReadWrite(renderInfo.frame);

				const std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer("BloomFrameBuffers(" + mipLevelString + ")");

				RenderPass::SubmitInfo submitInfo{};
				submitInfo.frame = renderInfo.frame;
				submitInfo.renderPass = renderInfo.renderPass;
				submitInfo.frameBuffer = frameBuffer;
				renderInfo.renderer->BeginRenderPass(submitInfo, "Bloom Down Sample [" + mipLevelString + "]", { 1.0f, 1.0f, 0.0f });

				glm::vec2 sourceSize = submitInfo.frameBuffer->GetSize();
				const std::shared_ptr<Buffer> mipBuffer = renderInfo.renderView->GetBuffer("BloomBuffers[" + mipLevelString + "]");
				baseMaterial->WriteToBuffer(mipBuffer, "MipBuffer", "sourceSize", sourceSize);
				baseMaterial->WriteToBuffer(mipBuffer, "MipBuffer", "bloomIntensity", bloomSettings.intensity);
				baseMaterial->WriteToBuffer(mipBuffer, "MipBuffer", "mipLevel", mipLevel);

				mipBuffer->Flush();

				const std::shared_ptr<UniformWriter> downUniformWriter = renderInfo.renderView->GetUniformWriter("BloomDownUniformWriters[" + mipLevelString + "]");
				downUniformWriter->WriteTextureToFrame("sourceTexture", sourceTexture);
				downUniformWriter->Flush();

				std::vector<NativeHandle> vertexBuffers;
				std::vector<size_t> vertexBufferOffsets;
				GetVertexBuffers(pipeline, plane, vertexBuffers, vertexBufferOffsets);

				renderInfo.renderer->Render(
					vertexBuffers,
					vertexBufferOffsets,
					plane->GetIndexBuffer()->GetNativeHandle(),
					plane->GetLods()[0].indexOffset * sizeof(uint32_t),
					plane->GetLods()[0].indexCount,
					pipeline,
					NativeHandle::Invalid(),
					0,
					1,
					{
						downUniformWriter->GetNativeHandle()
					},
					renderInfo.frame);

				renderInfo.renderer->EndRenderPass(submitInfo);

				sourceTexture = frameBuffer->GetAttachment(0);
			}
		}

		// Up Sample.
		{
			PROFILER_SCOPE("UpSample");

			const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
				std::filesystem::path("Materials") / "BloomUpSample.basemat");
			const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(renderPassName);
			if (!pipeline)
			{
				return;
			}

			// Create UniformWriters for up sample pass.
			if (recreateResources)
			{
				for (int mipLevel = 0; mipLevel < mipCount - 1; mipLevel++)
				{
					GetOrCreateUniformWriter(
						renderInfo.renderView,
						pipeline,
						Pipeline::DescriptorSetIndexType::RENDERER,
						renderPassName,
						"BloomUpUniformWriters[" + std::to_string(mipLevel) + "]");
				}
			}

			for (int mipLevel = mipCount - 1; mipLevel > 0; mipLevel--)
			{
				const std::string srcMipLevelString = std::to_string(mipLevel);
				const std::string dstMipLevelString = std::to_string(mipLevel - 1);

				renderInfo.renderer->MemoryBarrierFragmentReadWrite(renderInfo.frame);

				RenderPass::SubmitInfo submitInfo{};
				submitInfo.frame = renderInfo.frame;
				submitInfo.renderPass = renderInfo.renderPass;
				submitInfo.frameBuffer = renderInfo.renderView->GetFrameBuffer("BloomFrameBuffers(" + dstMipLevelString + ")");
				renderInfo.renderer->BeginRenderPass(submitInfo, "Bloom Up Sample [" + srcMipLevelString + "]", { 1.0f, 1.0f, 0.0f });

				const std::shared_ptr<UniformWriter> downUniformWriter = renderInfo.renderView->GetUniformWriter("BloomUpUniformWriters[" + dstMipLevelString + "]");
				downUniformWriter->WriteTextureToFrame("sourceTexture", renderInfo.renderView->GetFrameBuffer("BloomFrameBuffers(" + srcMipLevelString + ")")->GetAttachment(0));
				downUniformWriter->Flush();

				std::vector<NativeHandle> vertexBuffers;
				std::vector<size_t> vertexBufferOffsets;
				GetVertexBuffers(pipeline, plane, vertexBuffers, vertexBufferOffsets);

				renderInfo.renderer->Render(
					vertexBuffers,
					vertexBufferOffsets,
					plane->GetIndexBuffer()->GetNativeHandle(),
					plane->GetLods()[0].indexOffset * sizeof(uint32_t),
					plane->GetLods()[0].indexCount,
					pipeline,
					NativeHandle::Invalid(),
					0,
					1,
					{
						downUniformWriter->GetNativeHandle()
					},
					renderInfo.frame);

				renderInfo.renderer->EndRenderPass(submitInfo);
			}
		}
		renderInfo.renderer->EndCommandLabel(renderInfo.frame);
	};

	CreateRenderPass(createInfo);
}

