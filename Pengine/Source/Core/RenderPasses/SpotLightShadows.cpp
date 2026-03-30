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
#include "../Profiler.h"
#include "../Timer.h"

#include "../../Components/Camera.h"
#include "../../Components/DirectionalLight.h"
#include "../../Components/SpotLight.h"
#include "../../Components/Renderer3D.h"
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

void RenderPassManager::CreateSpotLightShadows()
{
	RenderPass::ClearDepth clearDepth{};
	clearDepth.clearDepth = 1.0f;
	clearDepth.clearStencil = 0;

	RenderPass::AttachmentDescription depth{};
	depth.textureCreateInfo.format = Format::D16_UNORM;
	depth.textureCreateInfo.aspectMask = Texture::AspectMask::DEPTH;
	depth.textureCreateInfo.instanceSize = sizeof(uint16_t);
	depth.textureCreateInfo.isMultiBuffered = true;
	depth.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::TRANSFER_DST, Texture::Usage::DEPTH_STENCIL_ATTACHMENT };
	depth.textureCreateInfo.name = "SpotLightShadows";
	depth.textureCreateInfo.filepath = depth.textureCreateInfo.name;
	depth.layout = Texture::Layout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth.load = RenderPass::Load::LOAD;
	depth.store = RenderPass::Store::STORE;

	Texture::SamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.filter = Texture::SamplerCreateInfo::Filter::NEAREST;
	samplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_WHITE;
	samplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_EDGE;
	samplerCreateInfo.maxAnisotropy = 1.0f;

	depth.textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = SpotLightShadows;
	createInfo.clearDepths = { clearDepth };
	createInfo.attachmentDescriptions = { depth };
	createInfo.resizeWithViewport = false;
	createInfo.createFrameBuffer = false;
	createInfo.isFrameBufferMultiBuffered = false;

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SpotLightShadows);

		const std::string& renderPassName = renderInfo.renderPass->GetName();

		const GraphicsSettings::Shadows::SpotLightShadows& spotLightShadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.spotLightShadows;
		if (!spotLightShadowsSettings.isEnabled || renderInfo.scene->GetGraphicsSettings().rayTracing.shadows.spotLight)
		{
			renderInfo.renderView->DeleteBuffer("InstanceBufferSpotLightShadows");
			renderInfo.renderView->DeleteBuffer("SpotLightShadowMapIndicesBuffer");
			renderInfo.renderView->DeleteFrameBuffer(renderPassName);

			return;
		}

		std::shared_ptr<Buffer> spotLightShadowMapIndicesBuffer = renderInfo.renderView->GetBuffer("SpotLightShadowMapIndicesBuffer");
		if (!spotLightShadowMapIndicesBuffer)
		{
			const std::shared_ptr<BaseMaterial> reflectionBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "DefaultReflection.basemat");
			const std::shared_ptr<Pipeline> pipeline = reflectionBaseMaterial->GetPipeline(DefaultReflection);
			if (!pipeline)
			{
				FATAL_ERROR("DefaultReflection base material is broken! No pipeline found!");
			}

			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, "Camera");
			spotLightShadowMapIndicesBuffer = GetOrCreateBuffer(
				renderInfo.renderView,
				renderUniformWriter,
				"SpotLightShadowMapIndicesBuffer",
				{},
				{ Buffer::Usage::UNIFORM_BUFFER },
				MemoryType::CPU, true);
		}

		std::vector<int> spotLightShadowMapIndicesData(32, -1);

		MultiPassLightData* multiPassData = (MultiPassLightData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassLightData");
		assert(multiPassData);
		
		size_t renderableCount = 0;
		const std::shared_ptr<Scene> scene = renderInfo.scene;
		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		const glm::vec3 cameraPosition = camera.GetEntity()->GetComponent<Transform>().GetPosition();
		entt::registry& registry = scene->GetRegistry();

		struct LightInfo
		{
			struct EntityData
			{
				std::shared_ptr<Mesh> mesh;
				uint32_t entityIndex;
				int lod;
			};

			std::unordered_map<std::shared_ptr<BaseMaterial>, std::unordered_map<std::shared_ptr<class Material>, std::vector<EntityData>>> renderableEntities;
			std::vector<entt::entity> entities;
			int lightIndex = -1;
			int shadowMapIndex = -1;
		};

		std::vector<LightInfo> lightInfos;

		const glm::ivec2 resolutions[4] = { { 1024, 1024 }, { 2048, 2048 }, { 3072, 3072 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapAtlasSize = resolutions[spotLightShadowsSettings.atlasQuality];

		const int faceSizes[4] = { 128, 256, 512, 1024 };
		const int faceSize = faceSizes[spotLightShadowsSettings.faceQuality];

		const float facesPerRow = (float)shadowMapAtlasSize.x / (float)faceSize;
		const int maxShadowMapCount = glm::floor(facesPerRow * facesPerRow);

		struct LightInfoToSort
		{
			int indexToLights;
			entt::entity entity;
			glm::vec3 position;
			float radius;
		};

		const std::array<glm::vec4, 6> cameraFrustumPlanes = Utils::GetFrustumPlanes(renderInfo.projection * camera.GetViewMat4());

		std::vector<LightInfoToSort> lights;
		lights.reserve(multiPassData->spotLights.size());

		for (size_t i = 0; i < multiPassData->spotLights.size(); i++)
		{
			if (lights.size() == 32)
			{
				break;
			}

			const auto& spotLight = multiPassData->spotLights[i];
			const Transform& transform = registry.get<Transform>(spotLight.entity);

			const SpotLight& sl = registry.get<SpotLight>(spotLight.entity);
			if (!sl.castShadows)
			{
				continue;
			}

			LightInfoToSort lightInfoToSort{};
			lightInfoToSort.indexToLights = i;
			lightInfoToSort.entity = spotLight.entity;
			lightInfoToSort.position = transform.GetPosition();
			lightInfoToSort.radius = sl.radius;

			if (Utils::IsSphereInsideFrustum(cameraFrustumPlanes, lightInfoToSort.position, lightInfoToSort.radius))
			{
				lights.emplace_back(lightInfoToSort);
			}
		}

		std::sort(lights.begin(), lights.end(),
		[&registry, &cameraPosition](const LightInfoToSort& first, const LightInfoToSort& second)
		{
			float firstDistance2 = 0.0f;
			{
				firstDistance2 = glm::distance2(cameraPosition, first.position) - (first.radius * first.radius);
			}

			float secondDistance2 = 0.0f;
			{
				secondDistance2 = glm::distance2(cameraPosition, second.position) - (second.radius * second.radius);
			}

			return firstDistance2 < secondDistance2;
		});

		int shadowMapIndex = 0;
		for (const auto& light : lights)
		{
			LightInfo lightInfo{};
			lightInfo.lightIndex = multiPassData->spotLights[light.indexToLights].index;

			SpotLight& sl = registry.get<SpotLight>(light.entity);

			if (sl.drawBoundingSphere)
			{
				const Transform& transform = registry.get<Transform>(light.entity);
				constexpr glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
				renderInfo.scene->GetVisualizer().DrawSphere(color, transform.GetTransform(), sl.radius, 10);
			}

			const auto frustumPlanes = Utils::GetFrustumPlanes(multiPassData->spotLights[light.indexToLights].viewProjectionMat4);

			std::vector<SceneBVH::BVHNode> bvhNodes;
			scene->GetBVH()->Traverse([&frustumPlanes, &bvhNodes](const SceneBVH::BVHNode& node)
			{
				if (!Utils::isAABBInsideFrustum(frustumPlanes, node.aabb.min, node.aabb.max))
				{
					return false;
				}

				if (node.IsLeaf())
				{
					bvhNodes.emplace_back(node);
				}

				return true;
			});

			// NOTE: BVH Culling is a lot slower than this for loop checks.
			for (const auto& node : bvhNodes)
			{
				if (Utils::IntersectAABBvsSphere(node.aabb.min, node.aabb.max, light.position, light.radius))
				{
					lightInfo.entities.emplace_back(node.entity);
				}
			}

			int localShadowMapIndex = -1;
			if (shadowMapIndex < maxShadowMapCount)
			{
				localShadowMapIndex = shadowMapIndex;
				shadowMapIndex++;
			}

			lightInfo.shadowMapIndex = localShadowMapIndex;
			spotLightShadowMapIndicesData[multiPassData->spotLights[light.indexToLights].index] = lightInfo.shadowMapIndex;

			if (lightInfo.shadowMapIndex > -1)
			{
				lightInfos.emplace_back(lightInfo);
			}
		}

		spotLightShadowMapIndicesBuffer->WriteToBuffer(spotLightShadowMapIndicesData.data(), spotLightShadowMapIndicesData.size() * sizeof(int));

		std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer(renderPassName);
		if (!frameBuffer)
		{
			const std::string& renderPassName = renderInfo.renderPass->GetName();
			frameBuffer = FrameBuffer::Create(renderInfo.renderPass, renderInfo.renderView.get(), shadowMapAtlasSize);

			renderInfo.renderView->SetFrameBuffer(renderPassName, frameBuffer);
		}

		if (frameBuffer->GetSize() != shadowMapAtlasSize)
		{
			frameBuffer->Resize(shadowMapAtlasSize);
		}

		for (LightInfo& lightInfo : lightInfos)
		{
			for (const entt::entity& entity : lightInfo.entities)
			{
				const Renderer3D* r3d = registry.try_get<Renderer3D>(entity);
				if (!r3d)
				{
					continue;
				}

				if (!r3d->castShadows)
				{
					continue;
				}

				if ((r3d->shadowVisibilityMask & camera.GetShadowVisibilityMask()) == 0)
				{
					continue;
				}

				if (!r3d->isEnabled || !registry.get<Transform>(entity).GetEntity()->IsEnabled())
				{
					continue;
				}

				if (!r3d->material || !r3d->material->IsPipelineEnabled(renderPassName))
				{
					continue;
				}

				const std::shared_ptr<Pipeline> pipeline = r3d->material->GetBaseMaterial()->GetPipeline(renderPassName);
				if (!pipeline)
				{
					continue;
				}

				const Transform& transform = registry.get<Transform>(entity);
				auto lod = GetLod(
					cameraPosition,
					transform.GetPosition(),
					glm::length(transform.GetScale() * glm::max(glm::abs(r3d->mesh->GetBoundingBox().min), glm::abs(r3d->mesh->GetBoundingBox().max))),
					r3d->mesh->GetLods());

				LightInfo::EntityData entityData{};
				entityData.entityIndex = r3d->entityIndex;
				entityData.mesh = r3d->mesh;
				entityData.lod = lod;

				lightInfo.renderableEntities[r3d->material->GetBaseMaterial()][r3d->material].emplace_back(entityData);

				renderableCount++;
			}
		}

		struct InstanceData
		{
			int entityIndex;
			int lightIndex;
		};

		std::shared_ptr<Buffer> instanceBuffer = renderInfo.renderView->GetBuffer("InstanceBufferSpotLightShadows");
		if ((renderableCount != 0 && !instanceBuffer) || (instanceBuffer && renderableCount != 0 && instanceBuffer->GetInstanceCount() < renderableCount))
		{
			Buffer::CreateInfo createInfo{};
			createInfo.instanceSize = sizeof(InstanceData);
			createInfo.instanceCount = renderableCount * 2;
			createInfo.usages = { Buffer::Usage::VERTEX_BUFFER };
			createInfo.memoryType = MemoryType::CPU;
			createInfo.isMultiBuffered = true;
			instanceBuffer = Buffer::Create(createInfo);

			renderInfo.renderView->SetBuffer("InstanceBufferSpotLightShadows", instanceBuffer);
		}

		struct ShadowMapViewportInfo
		{
			uint32_t textureWidth;
			uint32_t textureHeight;
			uint32_t faceSize;
		};

		auto getShadowMapFaceViewport = [](
			const ShadowMapViewportInfo& info,
			uint32_t shadowMapIndex)
		{
			RenderPass::Viewport viewport{};

			uint32_t totalIndex = shadowMapIndex;

			uint32_t facesPerRow = info.textureWidth / info.faceSize;
			uint32_t facesPerColumn = info.textureHeight / info.faceSize;

			uint32_t row = totalIndex / facesPerRow;
			uint32_t col = totalIndex % facesPerRow;

			viewport.position.x = static_cast<float>(col * info.faceSize);
			viewport.position.y = static_cast<float>(row * info.faceSize);
			viewport.size.x = static_cast<float>(info.faceSize);
			viewport.size.y = static_cast<float>(info.faceSize);
			viewport.minMaxDepth = { 0.0f, 1.0f };

			return viewport;
		};

		auto getShadowMapFaceScissor = [](
			const RenderPass::Viewport& viewport,
			uint32_t shadowMapIndex)
		{
			RenderPass::Scissors scissor{};
			scissor.offset.x = static_cast<int32_t>(viewport.position.x);
			scissor.offset.y = static_cast<int32_t>(viewport.position.y);
			scissor.size.x = static_cast<uint32_t>(viewport.size.x);
			scissor.size.y = static_cast<uint32_t>(viewport.size.y);

			return scissor;
		};

		ShadowMapViewportInfo shadowMapViewportInfo{};
		shadowMapViewportInfo.faceSize = faceSize;
		shadowMapViewportInfo.textureWidth = shadowMapAtlasSize.x;
		shadowMapViewportInfo.textureHeight = shadowMapAtlasSize.y;

		std::vector<InstanceData> instanceDatas;

		renderInfo.renderer->BeginCommandLabel(SpotLightShadows, topLevelRenderPassDebugColor, renderInfo.frame);

		renderInfo.renderer->BeginCommandLabel("ClearSpotLightShadowMapAtlas", { 1.0f, 1.0f, 0.0f }, renderInfo.frame);
		{
			RenderPass::ClearDepth clearDepth{};
			clearDepth.clearDepth = 1.0f;
			clearDepth.clearStencil = 0;
			renderInfo.renderer->ClearDepthStencilImage(frameBuffer->GetAttachment(0), clearDepth, renderInfo.frame);
		}
		renderInfo.renderer->EndCommandLabel(renderInfo.frame);

		for (const LightInfo& lightInfo : lightInfos)
		{
			RenderPass::SubmitInfo submitInfo{};
			submitInfo.frame = renderInfo.frame;
			submitInfo.renderPass = renderInfo.renderPass;
			submitInfo.frameBuffer = frameBuffer;
			submitInfo.viewport = getShadowMapFaceViewport(shadowMapViewportInfo, lightInfo.shadowMapIndex);
			submitInfo.scissors = getShadowMapFaceScissor(*submitInfo.viewport, lightInfo.shadowMapIndex);
			renderInfo.renderer->BeginRenderPass(submitInfo, "SpotLight", { 1.0f, 1.0f, 0.0f });

			for (const auto& [baseMaterial, entitiesByMaterial] : lightInfo.renderableEntities)
			{
				const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(renderPassName);
				if (!pipeline)
				{
					continue;
				}

				renderInfo.renderer->BindPipeline(pipeline, renderInfo.frame);

				for (const auto& [material, entities] : entitiesByMaterial)
				{
					std::vector<NativeHandle> uniformWriterNativeHandles;
					std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
					GetUniformWriters(pipeline, baseMaterial, material, renderInfo, uniformWriters, uniformWriterNativeHandles);
					if (!FlushUniformWriters(uniformWriters))
					{
						continue;
					}

					renderInfo.renderer->BindUniformWriters(pipeline, uniformWriterNativeHandles, 0, renderInfo.frame);

					for (const auto& entity : entities)
					{
						if (entity.entityIndex == -1)
							continue;

						const size_t instanceDataOffset = instanceDatas.size();
					
						InstanceData data{};
						data.entityIndex = entity.entityIndex;
						data.lightIndex = lightInfo.lightIndex;
						instanceDatas.emplace_back(data);
						
						std::vector<NativeHandle> vertexBuffers;
						std::vector<size_t> vertexBufferOffsets;
						renderInfo.renderer->BindVertexBuffers(
							vertexBuffers,
							vertexBufferOffsets,
							NativeHandle::Invalid(),
							0,
							instanceBuffer->GetNativeHandle(),
							instanceDataOffset * instanceBuffer->GetInstanceSize(),
							renderInfo.frame);

						Mesh::Lod lod = entity.mesh->GetLods()[entity.lod];

						renderInfo.renderer->Draw(
							lod.indexCount,
							1,
							lod.indexOffset,
							0,
							renderInfo.frame);
					}
				}
			}

			renderInfo.renderer->EndRenderPass(submitInfo);
		}

		renderInfo.renderer->EndCommandLabel(renderInfo.frame);

		// Because these are all just commands and will be rendered later we can write the instance buffer
		// just once when all instance data is collected.
		if (instanceBuffer && !instanceDatas.empty())
		{
			instanceBuffer->WriteToBuffer(instanceDatas.data(), instanceDatas.size() * sizeof(InstanceData));
			instanceBuffer->Flush();
		}
	};

	CreateRenderPass(createInfo);
}

