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

void RenderPassManager::CreateDecalPass()
{
	glm::vec4 clearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
	glm::vec4 clearShading = { 0.0f, 0.0f, 0.0f, 0.0f };
	glm::vec4 clearEmissive = { 0.0f, 0.0f, 0.0f, 0.0f };

	RenderPass::AttachmentDescription color = GetRenderPass(GBuffer)->GetAttachmentDescriptions()[0];
	color.load = RenderPass::Load::LOAD;
	color.store = RenderPass::Store::STORE;
	color.getFrameBufferCallback = [](RenderView* renderView)
	{
		return renderView->GetFrameBuffer(GBuffer)->GetAttachment(0);
	};

	RenderPass::AttachmentDescription shading = GetRenderPass(GBuffer)->GetAttachmentDescriptions()[2];
	shading.load = RenderPass::Load::LOAD;
	shading.store = RenderPass::Store::STORE;
	shading.getFrameBufferCallback = [](RenderView* renderView)
	{
		return renderView->GetFrameBuffer(GBuffer)->GetAttachment(2);
	};

	RenderPass::AttachmentDescription emissive = GetRenderPass(GBuffer)->GetAttachmentDescriptions()[3];
	emissive.load = RenderPass::Load::LOAD;
	emissive.store = RenderPass::Store::STORE;
	emissive.getFrameBufferCallback = [](RenderView* renderView)
	{
		return renderView->GetFrameBuffer(GBuffer)->GetAttachment(3);
	};

	Texture::SamplerCreateInfo emissiveSamplerCreateInfo{};
	emissiveSamplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER;
	emissiveSamplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_BLACK;
	emissiveSamplerCreateInfo.maxAnisotropy = 1.0f;

	emissive.textureCreateInfo.samplerCreateInfo = emissiveSamplerCreateInfo;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = Decals;
	createInfo.clearColors = { clearColor, clearShading, clearEmissive };
	createInfo.attachmentDescriptions = { color, shading, emissive };
	createInfo.resizeWithViewport = true;
	createInfo.resizeViewportScale = { 1.0f, 1.0f };

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(Decals);

		const std::string& renderPassName = renderInfo.renderPass->GetName();

		size_t renderableCount = 0;
		const std::shared_ptr<Scene> scene = renderInfo.scene;
		entt::registry& registry = scene->GetRegistry();
		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		const glm::mat4 viewProjectionMat4 = renderInfo.projection * camera.GetViewMat4();
		const auto r3dView = registry.view<Decal>();
		const auto unitCubeMesh = MeshManager::GetInstance().LoadMesh("UnitCube");

		std::unordered_map<std::shared_ptr<BaseMaterial>, std::unordered_map<std::shared_ptr<Material>, std::vector<entt::entity>>> renderableEntities;

		for (const entt::entity& entity : r3dView)
		{
			const Decal& decal = registry.get<Decal>(entity);
			if ((decal.objectVisibilityMask & camera.GetObjectVisibilityMask()) == 0)
			{
				continue;
			}

			const Transform& transform = registry.get<Transform>(entity);
			if (!transform.GetEntity()->IsEnabled())
			{
				continue;
			}

			if (!decal.material || !decal.material->IsPipelineEnabled(Decals))
			{
				continue;
			}

			const std::shared_ptr<Pipeline> pipeline = decal.material->GetBaseMaterial()->GetPipeline(renderPassName);
			if (!pipeline)
			{
				continue;
			}

			const glm::mat4& transformMat4 = transform.GetTransform();
			const BoundingBox& box = unitCubeMesh->GetBoundingBox();

			bool isInFrustum = FrustumCulling::CullBoundingBox(viewProjectionMat4, transformMat4, box.min, box.max, camera.GetZNear());
			if (!isInFrustum)
			{
				continue;
			}

			renderableEntities[decal.material->GetBaseMaterial()][decal.material].emplace_back(entity);

			renderableCount++;
		}

		struct DecalInstanceData
		{
			uint64_t materialBuffer;
			glm::mat4 transform;
			glm::mat4 inverseTransform;
		};

		std::shared_ptr<Buffer> instanceBuffer = renderInfo.renderView->GetBuffer("DecalInstanceBuffer");
		if ((renderableCount != 0 && !instanceBuffer) || (instanceBuffer && renderableCount != 0 && instanceBuffer->GetInstanceCount() < renderableCount))
		{
			Buffer::CreateInfo createInfo{};
			createInfo.instanceSize = sizeof(DecalInstanceData);
			createInfo.instanceCount = renderableCount * 2;
			createInfo.usages = { Buffer::Usage::VERTEX_BUFFER };
			createInfo.memoryType = MemoryType::CPU;
			createInfo.isMultiBuffered = true;
			instanceBuffer = Buffer::Create(createInfo);

			renderInfo.renderView->SetBuffer("DecalInstanceBuffer", instanceBuffer);
		}

		std::vector<DecalInstanceData> instanceDatas;

		const std::shared_ptr<BaseMaterial> decalBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "DecalBase.basemat");

		const std::shared_ptr<Pipeline> decalBasePipeline = decalBaseMaterial->GetPipeline(Decals);
		if (!decalBasePipeline)
		{
			return;
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, decalBasePipeline, Pipeline::DescriptorSetIndexType::RENDERER, Decals);

		const std::shared_ptr<Texture> depthTexture = renderInfo.renderView->GetFrameBuffer(GBuffer)->GetAttachment(4);
		const std::shared_ptr<Texture> normalTexture = renderInfo.renderView->GetFrameBuffer(GBuffer)->GetAttachment(1);
		renderUniformWriter->WriteTextureToFrame("depthGBufferTexture", depthTexture);
		renderUniformWriter->WriteTextureToFrame("normalGBufferTexture", normalTexture);

		const std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer(renderPassName);

		RenderPass::SubmitInfo submitInfo{};
		submitInfo.frame = renderInfo.frame;
		submitInfo.renderPass = renderInfo.renderPass;
		submitInfo.frameBuffer = frameBuffer;
		renderInfo.renderer->BeginRenderPass(submitInfo);

		for (const auto& [baseMaterial, entitiesByMaterial] : renderableEntities)
		{
			const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(renderPassName);
			if (!pipeline)
			{
				continue;
			}

			for (const auto& [material, entities] : entitiesByMaterial)
			{
				std::vector<NativeHandle> uniformWriterNativeHandles;
				std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
				GetUniformWriters(pipeline, baseMaterial, material, renderInfo, uniformWriters, uniformWriterNativeHandles);
				if (!FlushUniformWriters(uniformWriters))
				{
					continue;
				}

				const size_t instanceDataOffset = instanceDatas.size();

				for (const entt::entity& entity : entities)
				{
					DecalInstanceData data{};
					const Transform& transform = registry.get<Transform>(entity);
					data.transform = transform.GetTransform();
					data.inverseTransform = transform.GetInverseTransformMat4();
					data.materialBuffer = material->GetMaterialInfoBuffer()->GetDeviceAddress().Get();
					instanceDatas.emplace_back(data);
				}

				std::vector<NativeHandle> vertexBuffers;
				std::vector<size_t> vertexBufferOffsets;
				GetVertexBuffers(pipeline, unitCubeMesh, vertexBuffers, vertexBufferOffsets);

				renderInfo.renderer->Render(
					vertexBuffers,
					vertexBufferOffsets,
					unitCubeMesh->GetIndexBuffer()->GetNativeHandle(),
					unitCubeMesh->GetLods()[0].indexOffset * sizeof(uint32_t),
					unitCubeMesh->GetLods()[0].indexCount,
					pipeline,
					instanceBuffer->GetNativeHandle(),
					instanceDataOffset * instanceBuffer->GetInstanceSize(),
					entities.size(),
					uniformWriterNativeHandles,
					renderInfo.frame);
			}
		}

		// Because these are all just commands and will be rendered later we can write the instance buffer
		// just once when all instance data is collected.
		if (instanceBuffer && !instanceDatas.empty())
		{
			instanceBuffer->WriteToBuffer(instanceDatas.data(), instanceDatas.size() * sizeof(DecalInstanceData));
			instanceBuffer->Flush();
		}

		renderInfo.renderer->EndRenderPass(submitInfo);
	};

	CreateRenderPass(createInfo);
}

