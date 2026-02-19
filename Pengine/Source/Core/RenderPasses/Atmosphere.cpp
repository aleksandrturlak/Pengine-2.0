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

void RenderPassManager::CreateDefaultReflection()
{
	RenderPass::ClearDepth clearDepth{};
	clearDepth.clearDepth = 0.0f;
	clearDepth.clearStencil = 0;

	glm::vec4 clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	RenderPass::AttachmentDescription color{};
	color.textureCreateInfo.format = Format::R8G8B8A8_SRGB;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint8_t) * 4;
	color.textureCreateInfo.isMultiBuffered = true;
	color.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::COLOR_ATTACHMENT };
	color.textureCreateInfo.name = "DefaultReflectionColor";
	color.textureCreateInfo.filepath = color.textureCreateInfo.name;
	color.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = DefaultReflection;
	createInfo.clearColors = { clearColor };
	createInfo.clearDepths = { clearDepth };
	createInfo.attachmentDescriptions = { color };
	createInfo.createFrameBuffer = false;

	CreateRenderPass(createInfo);
}

void RenderPassManager::CreateAtmosphere()
{
	glm::vec4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

	RenderPass::AttachmentDescription color{};

	color.textureCreateInfo.format = Format::B10G11R11_UFLOAT_PACK32;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint32_t);
	color.textureCreateInfo.isMultiBuffered = true;
	color.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::COLOR_ATTACHMENT };
	color.textureCreateInfo.name = "AtmosphereColor";
	color.textureCreateInfo.filepath = color.textureCreateInfo.name;
	color.textureCreateInfo.size = { 256, 256 };
	color.textureCreateInfo.isCubeMap = true;
	color.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	color.load = RenderPass::Load::LOAD;
	color.store = RenderPass::Store::STORE;
	
	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = Atmosphere;
	createInfo.clearColors = { clearColor };
	createInfo.attachmentDescriptions = { color };
	createInfo.resizeWithViewport = false;

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(Atmosphere);

		const std::string& renderPassName = renderInfo.renderPass->GetName();
		std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.scene->GetRenderView()->GetFrameBuffer(Atmosphere);
		auto directionalLightView = renderInfo.scene->GetRegistry().view<DirectionalLight>();

		const bool hasDirectionalLight = !directionalLightView.empty();
		if (!hasDirectionalLight)
		{
			renderInfo.scene->GetRenderView()->DeleteFrameBuffer(renderPassName);

			return;
		}
		else
		{
			if (!frameBuffer)
			{
				frameBuffer = FrameBuffer::Create(renderInfo.renderPass, renderInfo.scene->GetRenderView().get(), {});
				renderInfo.scene->GetRenderView()->SetFrameBuffer(renderPassName, frameBuffer);
			}
		}

		const std::shared_ptr<Mesh> plane = MeshManager::GetInstance().GetMesh("FullScreenQuad");

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "Atmosphere.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(renderPassName);
		if (!pipeline)
		{
			return;
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.scene->GetRenderView(), pipeline, Pipeline::DescriptorSetIndexType::SCENE, renderPassName);
		const std::string atmosphereBufferName = "AtmosphereBuffer";
		const std::shared_ptr<Buffer> atmosphereBuffer = GetOrCreateBuffer(
			renderInfo.scene->GetRenderView(),
			renderUniformWriter,
			atmosphereBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		if (hasDirectionalLight)
		{
			const entt::entity& entity = directionalLightView.back();
			DirectionalLight& dl = renderInfo.scene->GetRegistry().get<DirectionalLight>(entity);
			const Transform& transform = renderInfo.scene->GetRegistry().get<Transform>(entity);

			baseMaterial->WriteToBuffer(atmosphereBuffer, "AtmosphereBuffer", "directionalLight.color", dl.color);
			baseMaterial->WriteToBuffer(atmosphereBuffer, "AtmosphereBuffer", "directionalLight.intensity", dl.intensity);
			baseMaterial->WriteToBuffer(atmosphereBuffer, "AtmosphereBuffer", "directionalLight.ambient", dl.ambient);

			const glm::vec3 directionWorldSpace = transform.GetForward();
			baseMaterial->WriteToBuffer(atmosphereBuffer, "AtmosphereBuffer", "directionalLight.directionWorldSpace", directionWorldSpace);

			int hasDirectionalLight = 1;
			baseMaterial->WriteToBuffer(atmosphereBuffer, "AtmosphereBuffer", "hasDirectionalLight", hasDirectionalLight);
		}
		else
		{
			int hasDirectionalLight = 0;
			baseMaterial->WriteToBuffer(atmosphereBuffer, "AtmosphereBuffer", "hasDirectionalLight", hasDirectionalLight);
		}

		const glm::vec2 faceSize = frameBuffer->GetSize();
		baseMaterial->WriteToBuffer(atmosphereBuffer, "AtmosphereBuffer", "faceSize", faceSize);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (!FlushUniformWriters(uniformWriters))
		{
			return;
		}
		
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

		// Update SkyBox.
		{
			std::shared_ptr<BaseMaterial> skyBoxBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
				std::filesystem::path("Materials") / "SkyBox.basemat");

			const std::shared_ptr<Pipeline> pipeline = skyBoxBaseMaterial->GetPipeline(GBuffer);
			if (pipeline)
			{
				const std::shared_ptr<UniformWriter> skyBoxUniformWriter = GetOrCreateUniformWriter(
					renderInfo.scene->GetRenderView(), pipeline, Pipeline::DescriptorSetIndexType::SCENE, "SkyBox");
				skyBoxUniformWriter->WriteTextureToFrame("SkyBox", frameBuffer->GetAttachment(0));
				skyBoxUniformWriter->Flush();
			}
		}
	};

	CreateRenderPass(createInfo);
}

