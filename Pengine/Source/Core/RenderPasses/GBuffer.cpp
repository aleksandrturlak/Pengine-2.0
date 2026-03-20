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

void RenderPassManager::CreateGBuffer()
{
	RenderPass::ClearDepth clearDepth{};
	clearDepth.clearDepth = 0.0f;
	clearDepth.clearStencil = 0;

	glm::vec4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	glm::vec4 clearNormal = { 0.0f, 0.0f, 0.0f, 0.0f };
	glm::vec4 clearShading = { 0.0f, 0.0f, 0.0f, 0.0f };
	glm::vec4 clearEmissive = { 0.0f, 0.0f, 0.0f, 0.0f };

	Texture::SamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER;
	samplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_BLACK;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.filter = Texture::SamplerCreateInfo::Filter::NEAREST;

	RenderPass::AttachmentDescription color{};
	color.textureCreateInfo.format = Format::B10G11R11_UFLOAT_PACK32;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint32_t);
	color.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::COLOR_ATTACHMENT };
	color.textureCreateInfo.name = "GBufferColor";
	color.textureCreateInfo.filepath = color.textureCreateInfo.name;
	color.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	color.load = RenderPass::Load::CLEAR;
	color.store = RenderPass::Store::STORE;
	color.textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

	RenderPass::AttachmentDescription normal{};
	normal.textureCreateInfo.format = Format::R16G16_SFLOAT;
	normal.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	normal.textureCreateInfo.instanceSize = sizeof(uint16_t) * 2;
	normal.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::STORAGE, Texture::Usage::COLOR_ATTACHMENT };
	normal.textureCreateInfo.name = "GBufferNormal";
	normal.textureCreateInfo.filepath = normal.textureCreateInfo.name;
	normal.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	normal.load = RenderPass::Load::CLEAR;
	normal.store = RenderPass::Store::STORE;
	normal.textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

	RenderPass::AttachmentDescription shading{};
	shading.textureCreateInfo.format = Format::R8G8B8A8_UNORM;
	shading.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	shading.textureCreateInfo.instanceSize = sizeof(uint8_t) * 4;
	shading.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::COLOR_ATTACHMENT };
	shading.textureCreateInfo.name = "GBufferShading";
	shading.textureCreateInfo.filepath = shading.textureCreateInfo.name;
	shading.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	shading.load = RenderPass::Load::CLEAR;
	shading.store = RenderPass::Store::STORE;
	shading.textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

	RenderPass::AttachmentDescription emissive{};
	emissive.textureCreateInfo.format = Format::B10G11R11_UFLOAT_PACK32;
	emissive.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	emissive.textureCreateInfo.instanceSize = sizeof(uint32_t);
	emissive.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::STORAGE, Texture::Usage::COLOR_ATTACHMENT };
	emissive.textureCreateInfo.name = "GBufferEmissive";
	emissive.textureCreateInfo.filepath = emissive.textureCreateInfo.name;
	emissive.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	emissive.load = RenderPass::Load::CLEAR;
	emissive.store = RenderPass::Store::STORE;
	emissive.textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

	RenderPass::AttachmentDescription depth{};
	depth.textureCreateInfo.format = Format::D32_SFLOAT;
	depth.textureCreateInfo.aspectMask = Texture::AspectMask::DEPTH;
	depth.textureCreateInfo.instanceSize = sizeof(float);
	depth.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::DEPTH_STENCIL_ATTACHMENT };
	depth.textureCreateInfo.name = "ZPrePassDepth";
	depth.textureCreateInfo.filepath = depth.textureCreateInfo.name;
	depth.layout = Texture::Layout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth.load = RenderPass::Load::CLEAR;
	depth.store = RenderPass::Store::STORE;

	Texture::SamplerCreateInfo depthSamplerCreateInfo{};
	depthSamplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER;
	depthSamplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_BLACK;

	depth.textureCreateInfo.samplerCreateInfo = depthSamplerCreateInfo;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = GBuffer;
	createInfo.clearColors = { clearColor, clearNormal, clearShading, clearEmissive };
	createInfo.clearDepths = { clearDepth };
	createInfo.attachmentDescriptions = { color, normal, shading, emissive, depth };
	createInfo.resizeWithViewport = true;
	createInfo.isFrameBufferMultiBuffered = false;
	createInfo.resizeViewportScale = { 1.0f, 1.0f };

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(GBuffer);
		const std::string& renderPassName = renderInfo.renderPass->GetName();

		LineRenderer* lineRenderer = (LineRenderer*)renderInfo.scene->GetRenderView()->GetCustomData("LineRenderer");
		if (!lineRenderer)
		{
			lineRenderer = new LineRenderer();

			renderInfo.scene->GetRenderView()->SetCustomData("LineRenderer", lineRenderer);
		}

		MultiPassEntityData* multiPassData = (MultiPassEntityData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassEntityData");

		const std::shared_ptr<Scene> scene = renderInfo.scene;
		entt::registry& registry = scene->GetRegistry();
		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		const glm::vec3 cameraPosition = camera.GetEntity()->GetComponent<Transform>().GetPosition();
		const glm::mat4 viewProjectionMat4 = renderInfo.projection * camera.GetViewMat4();

		if (scene->GetSettings().drawBoundingBoxes)
		{
			//const glm::mat4& transformMat4 = transform.GetTransform();
			//const BoundingBox& box = r3d.mesh->GetBoundingBox();
			//const glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);

			//scene->GetVisualizer().DrawBox(box.min, box.max, color, transformMat4);
			scene->GetBVH()->Traverse([scene](const SceneBVH::BVHNode& node)
				{
					scene->GetVisualizer().DrawBox(node.aabb.min, node.aabb.max, { 0.0f, 1.0f, 0.0f }, glm::mat4(1.0f));
					return true;
				});
		}

		const std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer(renderPassName);

		RenderPass::SubmitInfo submitInfo{};
		submitInfo.frame = renderInfo.frame;
		submitInfo.renderPass = renderInfo.renderPass;
		submitInfo.frameBuffer = frameBuffer;
		renderInfo.renderer->BeginRenderPass(submitInfo);

		if (multiPassData && multiPassData->passesByName.find(GBuffer) != multiPassData->passesByName.end())
		{
			const auto& gbufferData = multiPassData->passesByName[GBuffer];

			size_t offset = 0;
			size_t counterBufferOffset = 0;
			for (const auto& [id, pipeline] : gbufferData.sortedPipelines)
			{
				renderInfo.renderer->BindPipeline(pipeline, renderInfo.frame);

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
					const std::shared_ptr<Buffer>& indirectDrawCommandsBuffer = GetOrCreateBuffer(
						renderInfo.renderView, nullptr, "IndirectDrawCommands");
					const std::shared_ptr<Buffer>& indirectDrawCommandCountBuffer = GetOrCreateBuffer(
						renderInfo.renderView, nullptr, "IndirectDrawCommandCount");

					renderInfo.renderer->DrawIndirectCount(
						indirectDrawCommandsBuffer->GetNativeHandle(),
						offset,
						indirectDrawCommandCountBuffer->GetNativeHandle(),
						counterBufferOffset,
						gbufferData.pipelineInfos.at(pipeline).maxDrawCount,
						renderInfo.frame);

					offset += gbufferData.pipelineInfos.at(pipeline).maxDrawCount;
					counterBufferOffset++;
				}
			}
		}

		lineRenderer->Render(renderInfo);

		// Render SkyBox.
		if (!registry.view<DirectionalLight>().empty())
		{
			std::shared_ptr<Mesh> cubeMesh = MeshManager::GetInstance().LoadMesh("UnitCube");
			std::shared_ptr<BaseMaterial> skyBoxBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
				std::filesystem::path("Materials") / "SkyBox.basemat");

			const std::shared_ptr<Pipeline> pipeline = skyBoxBaseMaterial->GetPipeline(renderPassName);
			if (pipeline)
			{
				std::vector<NativeHandle> uniformWriterNativeHandles;
				std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
				GetUniformWriters(pipeline, skyBoxBaseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
				if (FlushUniformWriters(uniformWriters))
				{
					std::vector<NativeHandle> vertexBuffers;
					std::vector<size_t> vertexBufferOffsets;
					GetVertexBuffers(pipeline, cubeMesh, vertexBuffers, vertexBufferOffsets);

					renderInfo.renderer->Render(
						vertexBuffers,
						vertexBufferOffsets,
						cubeMesh->GetIndexBuffer()->GetNativeHandle(),
						cubeMesh->GetLods()[0].indexOffset * sizeof(uint32_t),
						cubeMesh->GetLods()[0].indexCount,
						pipeline,
						NativeHandle::Invalid(),
						0,
						1,
						uniformWriterNativeHandles,
						renderInfo.frame);
				}
			}
		}

		renderInfo.renderer->EndRenderPass(submitInfo);
	};

	CreateRenderPass(createInfo);
}

