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

void RenderPassManager::CreateToneMappingPass()
{
	glm::vec4 clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	RenderPass::AttachmentDescription color{};
	color.textureCreateInfo.format = Format::R8G8B8A8_SRGB;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint8_t) * 4;
	color.textureCreateInfo.isMultiBuffered = true;
	color.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::COLOR_ATTACHMENT };
	color.textureCreateInfo.name = "ToneMappedColor";
	color.textureCreateInfo.filepath = color.textureCreateInfo.name;
	color.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	color.load = RenderPass::Load::DONT_CARE;
	color.store = RenderPass::Store::STORE;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = ToneMapping;
	createInfo.clearColors = { clearColor };
	createInfo.attachmentDescriptions = { color };
	createInfo.resizeWithViewport = true;
	createInfo.resizeViewportScale = { 1.0f, 1.0f };

	createInfo.executeCallback = [](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(ToneMapping);

		const std::shared_ptr<Mesh> plane = MeshManager::GetInstance().LoadMesh("FullScreenQuad");

		const std::string& renderPassName = renderInfo.renderPass->GetName();

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial("Materials/ToneMapping.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(renderPassName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings& graphicsSettings = renderInfo.scene->GetGraphicsSettings();

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, renderPassName);
		const std::string toneMappingBufferBufferName = "ToneMappingBuffer";
		const std::shared_ptr<Buffer> toneMappingBufferBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			toneMappingBufferBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		const int isReflectionsEnabled = graphicsSettings.ssr.isEnabled || graphicsSettings.rayTracing.reflections.isRayTraced;
		const int reflectionsTextureMipLevelCount = isReflectionsEnabled ? renderInfo.renderView->GetStorageImage("BlurredReflections")->GetMipLevels() * graphicsSettings.ssr.mipMultiplier : 0;
		baseMaterial->WriteToBuffer(toneMappingBufferBuffer, toneMappingBufferBufferName, "toneMapperIndex", graphicsSettings.postProcess.toneMapper);
		baseMaterial->WriteToBuffer(toneMappingBufferBuffer, toneMappingBufferBufferName, "gamma", graphicsSettings.postProcess.gamma);
		baseMaterial->WriteToBuffer(toneMappingBufferBuffer, toneMappingBufferBufferName, "isReflectionsEnabled", isReflectionsEnabled);
		baseMaterial->WriteToBuffer(toneMappingBufferBuffer, toneMappingBufferBufferName, "reflectionsTextureMipLevelCount", reflectionsTextureMipLevelCount);

		toneMappingBufferBuffer->Flush();

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (!FlushUniformWriters(uniformWriters))
		{
			return;
		}

		const std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer(renderPassName);
		RenderPass::SubmitInfo submitInfo{};
		submitInfo.frame = renderInfo.frame;
		submitInfo.renderPass = renderInfo.renderPass;
		submitInfo.frameBuffer = frameBuffer;
		renderInfo.renderer->BeginRenderPass(submitInfo);

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
			uniformWriterNativeHandles,
			renderInfo.frame);

		renderInfo.renderer->EndRenderPass(submitInfo);
	};

	CreateRenderPass(createInfo);
}

void RenderPassManager::CreateAntiAliasingAndComposePass()
{
	glm::vec4 clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	RenderPass::AttachmentDescription color{};
	color.textureCreateInfo.format = Format::R8G8B8A8_SRGB;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint8_t) * 4;
	color.textureCreateInfo.isMultiBuffered = true;
	color.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::COLOR_ATTACHMENT };
	color.textureCreateInfo.name = "AntiAliasingAndCompose";
	color.textureCreateInfo.filepath = color.textureCreateInfo.name;
	color.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	color.load = RenderPass::Load::DONT_CARE;
	color.store = RenderPass::Store::STORE;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = AntiAliasingAndCompose;
	createInfo.clearColors = { clearColor };
	createInfo.attachmentDescriptions = { color };
	createInfo.resizeWithViewport = true;
	createInfo.resizeViewportScale = { 1.0f, 1.0f };

	createInfo.executeCallback = [](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(AntiAliasingAndCompose);

		const std::shared_ptr<Mesh> plane = MeshManager::GetInstance().LoadMesh("FullScreenQuad");

		const std::string& renderPassName = renderInfo.renderPass->GetName();

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial("Materials/AntiAliasingAndCompose.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(renderPassName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings& graphicsSettings = renderInfo.scene->GetGraphicsSettings();

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView,
			pipeline,
			Pipeline::DescriptorSetIndexType::RENDERER,
			renderPassName);
		const std::string postProcessBufferName = "PostProcessBuffer";
		const std::shared_ptr<Buffer> postProcessBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			postProcessBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		const glm::vec2 viewportSize = renderInfo.viewportSize;
		const int fxaa = graphicsSettings.postProcess.fxaa;
		baseMaterial->WriteToBuffer(postProcessBuffer, "PostProcessBuffer", "viewportSize", viewportSize);
		baseMaterial->WriteToBuffer(postProcessBuffer, "PostProcessBuffer", "fxaa", fxaa);

		postProcessBuffer->Flush();

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (!FlushUniformWriters(uniformWriters))
		{
			return;
		}

		const std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer(renderPassName);
		RenderPass::SubmitInfo submitInfo{};
		submitInfo.frame = renderInfo.frame;
		submitInfo.renderPass = renderInfo.renderPass;
		submitInfo.frameBuffer = frameBuffer;
		renderInfo.renderer->BeginRenderPass(submitInfo);

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
			uniformWriterNativeHandles,
			renderInfo.frame);

		renderInfo.renderer->EndRenderPass(submitInfo);
	};

	CreateRenderPass(createInfo);
}

bool RenderPassManager::FlushUniformWriters(const std::vector<std::shared_ptr<UniformWriter>>& uniformWriters)
{
	PROFILER_SCOPE(__FUNCTION__);

	for (const auto& uniformWriter : uniformWriters)
	{
		if (!uniformWriter)
		{
			return false;
		}

		uniformWriter->Flush();

		for (const auto& [name, buffers] : uniformWriter->GetBuffersByName())
		{
			for (const auto& buffer : buffers)
			{
				buffer->Flush();
			}
		}
	}

	return true;
}

void RenderPassManager::CreateUI()
{
	glm::vec4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

	RenderPass::AttachmentDescription color{};
	color.textureCreateInfo.format = Format::R8G8B8A8_UNORM;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint8_t) * 4;
	color.textureCreateInfo.isMultiBuffered = true;
	color.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::COLOR_ATTACHMENT };
	color.textureCreateInfo.name = "UIColor";
	color.textureCreateInfo.filepath = color.textureCreateInfo.name;
	color.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	color.load = RenderPass::Load::CLEAR;
	color.store = RenderPass::Store::STORE;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = UI;
	createInfo.clearColors = { clearColor };
	createInfo.attachmentDescriptions = { color };
	createInfo.resizeWithViewport = true;
	createInfo.resizeViewportScale = { 1.0f, 1.0f };

	createInfo.executeCallback = [](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(UI);
		
		UIRenderer* uiRenderer = (UIRenderer*)renderInfo.renderView->GetCustomData("UIRenderer");
		if (!uiRenderer)
		{
			uiRenderer = new UIRenderer();

			renderInfo.renderView->SetCustomData("UIRenderer", uiRenderer);
		}

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial("Materials/RectangleUI.basemat");
		const auto& view = renderInfo.scene->GetRegistry().view<Canvas>();
		std::vector<entt::entity> entities;
		entities.reserve(view.size());
		for (const auto& entity : view)
		{
			entities.emplace_back(entity);

			Canvas& canvas = renderInfo.scene->GetRegistry().get<Canvas>(entity);
			Renderer3D* r3d = renderInfo.scene->GetRegistry().try_get<Renderer3D>(entity);
			if (!canvas.drawInMainViewport)
			{
				if (canvas.size.x > 0 && canvas.size.y > 0)
				{
					if (!canvas.frameBuffer)
					{
						canvas.frameBuffer = FrameBuffer::Create(renderInfo.renderPass, nullptr, canvas.size);
					}
					else
					{
						if (canvas.size != canvas.frameBuffer->GetSize())
						{
							canvas.frameBuffer->Resize(canvas.size);
						}
					}

					if (r3d)
					{
						r3d->isEnabled = true;
						r3d->material->GetUniformWriter(GBuffer)->WriteTextureToFrame("albedoTexture", canvas.frameBuffer->GetAttachment(0));
					}
				}
			}
			else
			{
				if (canvas.frameBuffer)
				{
					r3d->isEnabled = false;
					r3d->material->GetUniformWriter(GBuffer)->WriteTextureToFrame("albedoTexture", TextureManager::GetInstance().GetWhite());
					canvas.frameBuffer = nullptr;
				}
			}
		}

		uiRenderer->Render(entities, baseMaterial, renderInfo);
	};

	CreateRenderPass(createInfo);
}

