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

void RenderPassManager::CreateTransparent()
{
	RenderPass::ClearDepth clearDepth{};
	clearDepth.clearDepth = 0.0f;
	clearDepth.clearStencil = 0;

	glm::vec4 clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	glm::vec4 clearNormal = { 0.0f, 0.0f, 0.0f, 0.0f };
	glm::vec4 clearShading = { 0.0f, 0.0f, 0.0f, 0.0f };
	glm::vec4 clearEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };

	RenderPass::AttachmentDescription color{};
	color.textureCreateInfo.format = Format::B10G11R11_UFLOAT_PACK32;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint32_t);
	color.textureCreateInfo.isMultiBuffered = true;
	color.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::STORAGE, Texture::Usage::COLOR_ATTACHMENT };
	color.textureCreateInfo.name = "DeferredColor";
	color.textureCreateInfo.filepath = color.textureCreateInfo.name;
	color.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	color.load = RenderPass::Load::LOAD;
	color.store = RenderPass::Store::STORE;

	RenderPass::AttachmentDescription normal = GetRenderPass(GBuffer)->GetAttachmentDescriptions()[1];
	normal.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	normal.load = RenderPass::Load::LOAD;
	normal.store = RenderPass::Store::STORE;
	normal.getFrameBufferCallback = [](RenderView* renderView)
	{
		return renderView->GetFrameBuffer(GBuffer)->GetAttachment(1);
	};

	RenderPass::AttachmentDescription shading = GetRenderPass(GBuffer)->GetAttachmentDescriptions()[2];
	shading.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	shading.load = RenderPass::Load::LOAD;
	shading.store = RenderPass::Store::STORE;
	shading.getFrameBufferCallback = [](RenderView* renderView)
	{
		return renderView->GetFrameBuffer(GBuffer)->GetAttachment(2);
	};

	RenderPass::AttachmentDescription emissive = GetRenderPass(GBuffer)->GetAttachmentDescriptions()[3];
	emissive.layout = Texture::Layout::COLOR_ATTACHMENT_OPTIMAL;
	emissive.load = RenderPass::Load::LOAD;
	emissive.store = RenderPass::Store::STORE;
	emissive.getFrameBufferCallback = [](RenderView* renderView)
	{
		return renderView->GetFrameBuffer(GBuffer)->GetAttachment(3);
	};

	RenderPass::AttachmentDescription depth = GetRenderPass(GBuffer)->GetAttachmentDescriptions()[4];
	depth.layout = Texture::Layout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth.load = RenderPass::Load::LOAD;
	depth.store = RenderPass::Store::STORE;
	depth.getFrameBufferCallback = [](RenderView* renderView)
	{
		return renderView->GetFrameBuffer(GBuffer)->GetAttachment(4);
	};

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = Transparent;
	createInfo.clearDepths = { clearDepth };
	createInfo.clearColors = { clearColor, clearNormal, clearShading, clearEmissive };
	createInfo.attachmentDescriptions = { color, normal, shading, emissive, depth };
	createInfo.resizeWithViewport = true;

	createInfo.executeCallback = [](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(Transparent);

		const std::string& renderPassName = renderInfo.renderPass->GetName();

		//VisibleData* visibleData = (VisibleData*)renderInfo.renderView->GetCustomData("VisibleData");
		const std::vector<entt::entity> visibleEntities;
		
		struct RenderData
		{
			Renderer3D r3d;
			glm::mat4 transformMat4;
			glm::mat4 rotationMat4;
			glm::mat3 inversetransformMat3;
			glm::vec3 scale;
			glm::vec3 position;
			entt::entity entity;
			float distance2ToCamera = 0.0f;
		};

		std::vector<std::vector<RenderData>> renderDatasByRenderingOrder;
		renderDatasByRenderingOrder.resize(11);

		size_t renderableCount = 0;
		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		const std::shared_ptr<Scene> scene = renderInfo.scene;
		entt::registry& registry = scene->GetRegistry();
		for (const entt::entity& entity : visibleEntities)
		{
			Renderer3D& r3d = registry.get<Renderer3D>(entity);
			Transform& transform = registry.get<Transform>(entity);

			if (!r3d.material->IsPipelineEnabled(renderPassName))
			{
				continue;
			}

			const std::shared_ptr<Pipeline> pipeline = r3d.material->GetBaseMaterial()->GetPipeline(renderPassName);
			if (!pipeline)
			{
				continue;
			}

			const glm::mat4& transformMat4 = transform.GetTransform();
			const BoundingBox& box = r3d.mesh->GetBoundingBox();

			if (scene->GetSettings().drawBoundingBoxes)
			{
				constexpr glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
				scene->GetVisualizer().DrawBox(box.min, box.max, color, transformMat4);
			}

			RenderData renderData{};
			renderData.entity = entity;
			renderData.r3d = r3d;
			renderData.transformMat4 = transformMat4;
			renderData.rotationMat4 = transform.GetRotationMat4();
			renderData.inversetransformMat3 = transform.GetInverseTransform();
			renderData.scale = transform.GetScale();
			renderData.position = transform.GetPosition();

			renderDatasByRenderingOrder[r3d.renderingOrder].emplace_back(renderData);

			renderableCount++;
		}

		if (renderableCount == 0)
		{
			return;
		}

		const glm::vec3 cameraPosition = renderInfo.camera->GetComponent<Transform>().GetPosition();

		for (auto& renderDataByRenderingOrder : renderDatasByRenderingOrder)
		{
			for (RenderData& renderData : renderDataByRenderingOrder)
			{
				const glm::vec3 boundingBoxWorldPosition = renderData.position + renderData.r3d.mesh->GetBoundingBox().offset * renderData.scale;
				const glm::vec3 direction = glm::normalize((boundingBoxWorldPosition) - cameraPosition);

				Raycast::Hit hit{};
				if (Raycast::IntersectBoxOBB(
					cameraPosition,
					direction,
					renderData.r3d.mesh->GetBoundingBox().min,
					renderData.r3d.mesh->GetBoundingBox().max,
					renderData.position,
					renderData.scale,
					renderData.rotationMat4,
					FLT_MAX,
					hit))
				{
					renderData.distance2ToCamera = glm::distance2(cameraPosition, hit.point);
				}
				else
				{
					renderData.distance2ToCamera = glm::distance2(cameraPosition, renderData.position);
				}
			}

			auto isFurther = [cameraPosition](const RenderData& a, const RenderData& b)
			{
				return a.distance2ToCamera > b.distance2ToCamera;
			};

			std::sort(renderDataByRenderingOrder.begin(), renderDataByRenderingOrder.end(), isFurther);
		}

		std::shared_ptr<Buffer> instanceBuffer = renderInfo.renderView->GetBuffer("InstanceBufferTransparent");
		if ((renderableCount != 0 && !instanceBuffer) || (instanceBuffer && renderableCount != 0 && instanceBuffer->GetInstanceCount() < renderableCount))
		{
			Buffer::CreateInfo createInfo{};
			createInfo.instanceSize = sizeof(InstanceData);
			createInfo.instanceCount = renderableCount * 2;
			createInfo.usages = { Buffer::Usage::VERTEX_BUFFER };
			createInfo.memoryType = MemoryType::CPU;
			createInfo.isMultiBuffered = true;
			instanceBuffer = Buffer::Create(createInfo);

			renderInfo.renderView->SetBuffer("InstanceBufferTransparent", instanceBuffer);
		}

		std::vector<InstanceData> instanceDatas;

		const std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer(renderPassName);
		RenderPass::SubmitInfo submitInfo{};
		submitInfo.frame = renderInfo.frame;
		submitInfo.renderPass = renderInfo.renderPass;
		submitInfo.frameBuffer = frameBuffer;
		renderInfo.renderer->BeginRenderPass(submitInfo);

		for (auto& renderDataByRenderingOrder : renderDatasByRenderingOrder)
		{
			for (const auto& renderData : renderDataByRenderingOrder)
			{
				const std::shared_ptr<Pipeline> pipeline = renderData.r3d.material->GetBaseMaterial()->GetPipeline(renderPassName);

				const size_t instanceDataOffset = instanceDatas.size();

				InstanceData data{};
				data.materialBuffer = renderData.r3d.material->GetBuffer("MaterialBuffer")->GetDeviceAddress().Get();
				data.transform = renderData.transformMat4;
				data.inverseTransform = glm::transpose(renderData.inversetransformMat3);
				instanceDatas.emplace_back(data);

				// Can be done more optimal I guess.
				std::vector<NativeHandle> uniformWriterNativeHandles;
				std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
				GetUniformWriters(
					pipeline,
					renderData.r3d.material->GetBaseMaterial(),
					renderData.r3d.material,
					renderInfo,
					uniformWriters,
					uniformWriterNativeHandles);
				
				SkeletalAnimator* skeletalAnimator = nullptr;
				const Renderer3D& r3d = registry.get<Renderer3D>(renderData.entity);
				if (const auto skeletalAnimatorEntity = scene->FindEntityByUUID(r3d.skeletalAnimatorEntityUUID))
				{
					skeletalAnimator = registry.try_get<SkeletalAnimator>(skeletalAnimatorEntity->GetHandle());
				}

				// if (skeletalAnimator)
				// {
				// 	uniformWriters.emplace_back(skeletalAnimator->GetUniformWriter());
				// }
				
				if (!FlushUniformWriters(uniformWriters))
				{
					continue;
				}

				std::vector<NativeHandle> vertexBuffers;
				std::vector<size_t> vertexBufferOffsets;
				GetVertexBuffers(pipeline, renderData.r3d.mesh, vertexBuffers, vertexBufferOffsets);

				renderInfo.renderer->Render(
					vertexBuffers,
					vertexBufferOffsets,
					renderData.r3d.mesh->GetIndexBuffer()->GetNativeHandle(),
					renderData.r3d.mesh->GetLods()[0].indexOffset * sizeof(uint32_t),
					renderData.r3d.mesh->GetLods()[0].indexCount,
					pipeline,
					instanceBuffer->GetNativeHandle(),
					instanceDataOffset * instanceBuffer->GetInstanceSize(),
					1,
					uniformWriterNativeHandles,
					renderInfo.frame);
			}
		}

		// Because these are all just commands and will be rendered later we can write the instance buffer
		// just once when all instance data is collected.
		if (instanceBuffer && !instanceDatas.empty())
		{
			instanceBuffer->WriteToBuffer(instanceDatas.data(), instanceDatas.size() * sizeof(InstanceData));
			instanceBuffer->Flush();
		}

		renderInfo.renderer->EndRenderPass(submitInfo);
	};

	CreateRenderPass(createInfo);
}

