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

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SpotLightShadows);

		const std::string& renderPassName = renderInfo.renderPass->GetName();

		size_t renderableCount = 0;
		const std::shared_ptr<Scene> scene = renderInfo.scene;
		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		const glm::vec3 cameraPosition = camera.GetEntity()->GetComponent<Transform>().GetPosition();
		entt::registry& registry = scene->GetRegistry();

		struct LightInfo
		{
			std::vector<entt::entity> entities;
			RenderableEntities renderableEntities;
			int lightIndex = -1;
			int shadowMapIndex = -1;
		};

		std::vector<LightInfo> lightInfos;

		const GraphicsSettings::Shadows::SpotLightShadows& spotLightShadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.spotLightShadows;
		const glm::ivec2 resolutions[4] = { { 1024, 1024 }, { 2048, 2048 }, { 3072, 3072 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapAtlasSize = resolutions[spotLightShadowsSettings.atlasQuality];

		const int faceSizes[4] = { 128, 256, 512, 1024 };
		const int faceSize = faceSizes[spotLightShadowsSettings.faceQuality];

		const float facesPerRow = (float)shadowMapAtlasSize.x / (float)faceSize;
		const int maxShadowMapCount = glm::floor(facesPerRow * facesPerRow);

		const std::shared_ptr<BaseMaterial> deferredBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "Deferred.basemat");
		const std::shared_ptr<Pipeline> deferredPipeline = deferredBaseMaterial->GetPipeline(Deferred);
		if (!deferredPipeline)
		{
			return;
		}

		const std::string lightsBufferName = "Lights";
		const std::shared_ptr<UniformWriter> lightsUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, deferredPipeline, Pipeline::DescriptorSetIndexType::RENDERER, lightsBufferName);
		const std::shared_ptr<Buffer> lightsBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			lightsUniformWriter,
			lightsBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		const int isSpotLightShadowsEnabled = spotLightShadowsSettings.isEnabled;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightShadows.isEnabled", isSpotLightShadowsEnabled);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightShadows.shadowMapAtlasSize", shadowMapAtlasSize.x);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightShadows.faceSize", faceSize);

		const auto view = registry.view<SpotLight>();

		struct LightInfoToSort
		{
			entt::entity entity;
			glm::vec3 position;
			float radius;
		};

		const std::array<glm::vec4, 6> cameraFrustumPlanes = Utils::GetFrustumPlanes(renderInfo.projection * camera.GetViewMat4());

		std::vector<LightInfoToSort> lights;
		lights.reserve(view.size());

		for (size_t i = 0; i < view.size(); i++)
		{
			Transform& transform = registry.get<Transform>(view[i]);
			if (!transform.GetEntity()->IsEnabled())
			{
				continue;
			}

			LightInfoToSort lightInfoToSort{};
			lightInfoToSort.entity = view[i];
			lightInfoToSort.position = transform.GetPosition();
			lightInfoToSort.radius = registry.get<SpotLight>(view[i]).radius;

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

		int lightIndex = 0;
		int shadowMapIndex = 0;
		for (const auto& light : lights)
		{
			if (lightIndex == 32)
			{
				break;
			}

			LightInfo& lightInfo = lightInfos.emplace_back();
			lightInfo.lightIndex = lightIndex;

			SpotLight& sl = registry.get<SpotLight>(light.entity);
			Transform& transform = registry.get<Transform>(light.entity);

			const glm::vec3 lightPositionWorldSpace = light.position;
			const glm::vec3 lightPositionViewSpace = camera.GetViewMat4() * glm::vec4(light.position, 1.0f);
			const glm::vec3 directionViewSpace = glm::mat3(camera.GetViewMat4()) * transform.GetForward();
			const int castSSS = sl.castSSS;
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].positionWorldSpace", lightIndex), lightPositionWorldSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].positionViewSpace", lightIndex), lightPositionViewSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].directionViewSpace", lightIndex), directionViewSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].castSSS", lightIndex), castSSS);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].color", lightIndex), sl.color);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].intensity", lightIndex), sl.intensity);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].radius", lightIndex), sl.radius);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].bias", lightIndex), sl.bias);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].innerCutOff", lightIndex), sl.innerCutOff);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].outerCutOff", lightIndex), sl.outerCutOff);

			if (sl.drawBoundingSphere)
			{
				constexpr glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
				renderInfo.scene->GetVisualizer().DrawSphere(color, transform.GetTransform(), sl.radius, 10);
			}

			int slShadowMapIndex = -1;
			if (spotLightShadowsSettings.isEnabled && sl.castShadows)
			{
				if (shadowMapIndex < maxShadowMapCount)
				{
					slShadowMapIndex = shadowMapIndex;
					shadowMapIndex++;
				}

				const glm::mat4 viewProjectionMat4 = glm::perspective(
					sl.outerCutOff * 2.0f,
					1.0f,
					camera.GetZNear(),
					sl.radius) * transform.GetInverseTransformMat4();

				const auto frustumPlanes = Utils::GetFrustumPlanes(viewProjectionMat4);

				std::vector<SceneBVH::BVHNode> bvhNodes;
				scene->GetBVH()->Traverse([&frustumPlanes, &bvhNodes](const SceneBVH::BVHNode& node)
				{
					if (!Utils::isAABBInsideFrustum(frustumPlanes, node.aabb.min, node.aabb.max))
					{
						return false;
					}

					if (node.IsLeaf() && node.entity->IsValid())
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
						lightInfo.entities.emplace_back(node.entity->GetHandle());
					}
				}

				deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].viewProjectionMat4", lightIndex), viewProjectionMat4);
			}

			lightInfo.shadowMapIndex = slShadowMapIndex;
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].shadowMapIndex", lightIndex), slShadowMapIndex);

			lightIndex++;
		}

		int spotLightsCount = lightInfos.size();
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightsCount", spotLightsCount);

		if (!spotLightShadowsSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(renderPassName);
			renderInfo.renderView->DeleteBuffer("InstanceBufferSpotLightShadows");
			renderInfo.renderView->DeleteFrameBuffer(renderPassName);

			return;
		}

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
				const Renderer3D& r3d = registry.get<Renderer3D>(entity);

				if (!r3d.castShadows)
				{
					continue;
				}

				if ((r3d.shadowVisibilityMask & camera.GetShadowVisibilityMask()) == 0)
				{
					continue;
				}

				if (!r3d.material || !r3d.material->IsPipelineEnabled(renderPassName))
				{
					continue;
				}

				const std::shared_ptr<Pipeline> pipeline = r3d.material->GetBaseMaterial()->GetPipeline(renderPassName);
				if (!pipeline)
				{
					continue;
				}

				const Transform& transform = registry.get<Transform>(entity);
				auto lod = GetLod(
					cameraPosition,
					transform.GetPosition(),
					glm::length(transform.GetScale() * glm::max(glm::abs(r3d.mesh->GetBoundingBox().min), glm::abs(r3d.mesh->GetBoundingBox().max))),
					r3d.mesh->GetLods());

				// if (r3d.mesh->GetType() == Mesh::Type::SKINNED)
				// {
				// 	if (const auto skeletalAnimatorEntity = scene->FindEntityByUUID(r3d.skeletalAnimatorEntityUUID))
				// 	{
				// 		SkeletalAnimator* skeletalAnimator = registry.try_get<SkeletalAnimator>(skeletalAnimatorEntity->GetHandle());
				// 		if (skeletalAnimator)
				// 		{
				// 			UpdateSkeletalAnimator(skeletalAnimator, r3d.material->GetBaseMaterial(), pipeline);
				// 		}
				// 	}

				// 	EntitiesByMesh::Single single{};
				// 	single.entity = entity;
				// 	single.mesh = r3d.mesh;
				// 	single.lod = lod;

				// 	lightInfo.renderableEntities[r3d.material->GetBaseMaterial()][r3d.material].single.emplace_back(single);
				// }
				// else if (r3d.mesh->GetType() == Mesh::Type::STATIC)
				{
					auto& entities = lightInfo.renderableEntities[r3d.material->GetBaseMaterial()][r3d.material].instanced[r3d.mesh];
					entities.resize(r3d.mesh->GetLods().size());
					entities[lod].reserve(50);
					entities[lod].emplace_back(entity);
				}

				renderableCount++;
			}
		}

		struct InstanceData
		{
			glm::mat4 transform;
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

			// Render all base materials -> materials -> meshes | put gameobjects into the instance buffer.
			for (const auto& [baseMaterial, meshesByMaterial] : lightInfo.renderableEntities)
			{
				const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(renderPassName);
				if (!pipeline)
				{
					continue;
				}

				for (const auto& [material, gameObjectsByMeshes] : meshesByMaterial)
				{
					std::vector<NativeHandle> uniformWriterNativeHandles;
					std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
					GetUniformWriters(pipeline, baseMaterial, material, renderInfo, uniformWriters, uniformWriterNativeHandles);
					if (!FlushUniformWriters(uniformWriters))
					{
						continue;
					}

					for (const auto& [mesh, entitiesByLod] : gameObjectsByMeshes.instanced)
					{
						for (size_t lod = 0; lod < entitiesByLod.size(); lod++)
						{
							if (entitiesByLod[lod].empty()) continue;

							const size_t instanceDataOffset = instanceDatas.size();

							for (const entt::entity& entity : entitiesByLod[lod])
							{
								InstanceData data{};
								const Transform& transform = registry.get<Transform>(entity);
								data.transform = transform.GetTransform();
								data.lightIndex = lightInfo.lightIndex;
								instanceDatas.emplace_back(data);
							}

							std::vector<NativeHandle> vertexBuffers;
							std::vector<size_t> vertexBufferOffsets;
							GetVertexBuffers(pipeline, mesh, vertexBuffers, vertexBufferOffsets);

							renderInfo.renderer->Render(
								vertexBuffers,
								vertexBufferOffsets,
								mesh->GetIndexBuffer()->GetNativeHandle(),
								mesh->GetLods()[lod].indexOffset * sizeof(uint32_t),
								mesh->GetLods()[lod].indexCount,
								pipeline,
								instanceBuffer->GetNativeHandle(),
								instanceDataOffset * instanceBuffer->GetInstanceSize(),
								entitiesByLod[lod].size(),
								uniformWriterNativeHandles,
								renderInfo.frame);
						}
					}

					for (const auto& single : gameObjectsByMeshes.single)
					{
						const size_t instanceDataOffset = instanceDatas.size();

						InstanceData data{};
						const Transform& transform = registry.get<Transform>(single.entity);
						data.transform = transform.GetTransform();
						data.lightIndex = lightInfo.lightIndex;
						instanceDatas.emplace_back(data);

						SkeletalAnimator* skeletalAnimator = nullptr;
						const Renderer3D& r3d = registry.get<Renderer3D>(single.entity);
						if (const auto skeletalAnimatorEntity = scene->FindEntityByUUID(r3d.skeletalAnimatorEntityUUID))
						{
							skeletalAnimator = registry.try_get<SkeletalAnimator>(skeletalAnimatorEntity->GetHandle());
						}

						// if (skeletalAnimator)
						// {
						// 	std::vector<NativeHandle> newUniformWriterNativeHandles = uniformWriterNativeHandles;
						// 	uniformWriterNativeHandles.emplace_back(skeletalAnimator->GetUniformWriter()->GetNativeHandle());

						// 	std::vector<NativeHandle> vertexBuffers;
						// 	std::vector<size_t> vertexBufferOffsets;
						// 	GetVertexBuffers(pipeline, single.mesh, vertexBuffers, vertexBufferOffsets);

						// 	renderInfo.renderer->Render(
						// 		vertexBuffers,
						// 		vertexBufferOffsets,
						// 		single.mesh->GetIndexBuffer()->GetNativeHandle(),
						// 		single.mesh->GetLods()[single.lod].indexOffset * sizeof(uint32_t),
						// 		single.mesh->GetLods()[single.lod].indexCount,
						// 		pipeline,
						// 		instanceBuffer->GetNativeHandle(),
						// 		instanceDataOffset * instanceBuffer->GetInstanceSize(),
						// 		1,
						// 		newUniformWriterNativeHandles,
						// 		renderInfo.frame);
						// }
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

