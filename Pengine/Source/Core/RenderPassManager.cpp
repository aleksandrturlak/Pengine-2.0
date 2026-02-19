#include "RenderPassManager.h"

#include "Logger.h"
#include "BindlessUniformWriter.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Time.h"
#include "FrustumCulling.h"
#include "Raycast.h"
#include "UIRenderer.h"
#include "Profiler.h"
#include "Timer.h"

#include "../Components/Canvas.h"
#include "../Components/Camera.h"
#include "../Components/Decal.h"
#include "../Components/DirectionalLight.h"
#include "../Components/PointLight.h"
#include "../Components/SpotLight.h"
#include "../Components/Renderer3D.h"
#include "../Components/SkeletalAnimator.h"
#include "../Components/Transform.h"
#include "../Graphics/Device.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/RenderView.h"
#include "../Graphics/GraphicsPipeline.h"
#include "../EventSystem/EventSystem.h"
#include "../EventSystem/NextFrameEvent.h"

#include "../Core/ViewportManager.h"
#include "../Core/Viewport.h"

using namespace Pengine;

RenderPassManager& RenderPassManager::GetInstance()
{
	static RenderPassManager renderPassManager;
	return renderPassManager;
}

std::shared_ptr<RenderPass> RenderPassManager::CreateRenderPass(const RenderPass::CreateInfo& createInfo)
{
	assert(createInfo.type == Pass::Type::GRAPHICS);

	std::shared_ptr<RenderPass> renderPass = RenderPass::Create(createInfo);
	m_PassesByName.emplace(createInfo.name, renderPass);

	Logger::Log(std::format("Created Render Pass: {} {}", renderPass->GetName(), renderPass->GetId()), BOLDGREEN);

	return renderPass;
}

std::shared_ptr<ComputePass> RenderPassManager::CreateComputePass(const ComputePass::CreateInfo& createInfo)
{
	assert(createInfo.type == Pass::Type::COMPUTE);

	std::shared_ptr<ComputePass> computePass = std::make_shared<ComputePass>(createInfo);
	m_PassesByName.emplace(createInfo.name, computePass);

	Logger::Log(std::format("Created Compute Pass: {} {}", computePass->GetName(), computePass->GetId()), BOLDGREEN);

	return computePass;
}

std::shared_ptr<Pass> RenderPassManager::GetPass(const std::string& name) const
{
	if (const auto passByName = m_PassesByName.find(name);
		passByName != m_PassesByName.end())
	{
		return passByName->second;
	}

	FATAL_ERROR(name + " id of render pass doesn't exist, please create render pass!");

	return nullptr;
}

std::shared_ptr<RenderPass> RenderPassManager::GetRenderPass(const std::string& name) const
{
	std::shared_ptr<Pass> pass = GetPass(name);

	if (pass->GetType() == Pass::Type::GRAPHICS)
	{
		return std::dynamic_pointer_cast<RenderPass>(pass);
	}

	FATAL_ERROR(name + " id of render pass doesn't have type GRAPHICS!");

	return nullptr;
}

std::shared_ptr<ComputePass> RenderPassManager::GetComputePass(const std::string& name) const
{
	std::shared_ptr<Pass> pass = GetPass(name);

	if (pass->GetType() == Pass::Type::COMPUTE)
	{
		return std::dynamic_pointer_cast<ComputePass>(pass);
	}

	FATAL_ERROR(name + " id of render pass doesn't have type COMPUTE!");

	return nullptr;
}

void RenderPassManager::ShutDown()
{
	m_PassesByName.clear();
}

std::shared_ptr<UniformWriter> RenderPassManager::ResolveUniformWriter(
	Pipeline::DescriptorSetIndexType type,
	const std::string& name,
	const std::shared_ptr<BaseMaterial>& baseMaterial,
	const std::shared_ptr<Material>& material,
	const RenderPass::RenderCallbackInfo& renderInfo,
	const std::shared_ptr<UniformWriter>& objectUniformWriter)
{
	switch (type)
	{
	case Pipeline::DescriptorSetIndexType::BINDLESS:
		return BindlessUniformWriter::GetInstance().GetBindlessUniformWriter();
	case Pipeline::DescriptorSetIndexType::RENDERER:
		return renderInfo.renderView->GetUniformWriter(name);
	case Pipeline::DescriptorSetIndexType::SCENE:
		return renderInfo.scene->GetRenderView()->GetUniformWriter(name);
	case Pipeline::DescriptorSetIndexType::RENDERPASS:
		return RenderPassManager::GetInstance().GetRenderPass(name)->GetUniformWriter();
	case Pipeline::DescriptorSetIndexType::BASE_MATERIAL:
		return baseMaterial ? baseMaterial->GetUniformWriter(name) : nullptr;
	case Pipeline::DescriptorSetIndexType::MATERIAL:
		return material ? material->GetUniformWriter(name) : nullptr;
	case Pipeline::DescriptorSetIndexType::OBJECT:
		return objectUniformWriter;
	default:
		return nullptr;
	}
}

void RenderPassManager::GetUniformWriters(
	std::shared_ptr<Pipeline> pipeline,
	std::shared_ptr<BaseMaterial> baseMaterial,
	std::shared_ptr<Material> material,
	const RenderPass::RenderCallbackInfo& renderInfo,
	std::vector<std::shared_ptr<UniformWriter>>& uniformWriters,
	std::vector<NativeHandle>& uniformWriterNativeHandles)
{
	PROFILER_SCOPE(__FUNCTION__);

	for (const auto& [set, location] : pipeline->GetSortedDescriptorSets())
	{
		if (const std::shared_ptr<UniformWriter> uniformWriter = ResolveUniformWriter(
			location.first, location.second, baseMaterial, material, renderInfo, nullptr))
		{
			uniformWriters.emplace_back(uniformWriter);
		}
	}

	for (const auto& uniformWriter : uniformWriters)
	{
		uniformWriterNativeHandles.emplace_back(uniformWriter->GetNativeHandle());
	}
}

bool RenderPassManager::BindAndFlushUniformWriters(
	std::shared_ptr<Pipeline> pipeline,
	std::shared_ptr<BaseMaterial> baseMaterial,
	std::shared_ptr<Material> material,
	const RenderPass::RenderCallbackInfo& renderInfo,
	const std::vector<Pipeline::DescriptorSetIndexType>& descriptorSetIndexTypes,
	std::shared_ptr<UniformWriter> objectUniformWriter)
{
	PROFILER_SCOPE(__FUNCTION__);

	for (const auto& type : descriptorSetIndexTypes)
	{
		const auto descriptorSet = pipeline->GetDescriptorSetIndexByType(type);
		for (const auto& [name, set] : descriptorSet)
		{
			const std::shared_ptr<UniformWriter> uniformWriter = ResolveUniformWriter(
				type, name, baseMaterial, material, renderInfo, objectUniformWriter);
			if (!uniformWriter) continue;

			if (!FlushUniformWriters({ uniformWriter }))
			{
				return false;
			}

			renderInfo.renderer->BindUniformWriters(pipeline, { uniformWriter->GetNativeHandle() }, set, renderInfo.frame);
		}
	}

	return true;
}

void RenderPassManager::PrepareUniformsPerViewportBeforeDraw(const RenderPass::RenderCallbackInfo& renderInfo)
{
	PROFILER_SCOPE(__FUNCTION__);

	const std::shared_ptr<BaseMaterial> reflectionBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
		std::filesystem::path("Materials") / "DefaultReflection.basemat");
	const std::shared_ptr<Pipeline> pipeline = reflectionBaseMaterial->GetPipeline(DefaultReflection);
	if (!pipeline)
	{
		FATAL_ERROR("DefaultReflection base material is broken! No pipeline found!");
	}

	const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, DefaultReflection);
	const std::string globalBufferName = "GlobalBuffer";
	const std::shared_ptr<Buffer> globalBuffer = GetOrCreateBuffer(
		renderInfo.renderView,
		renderUniformWriter,
		globalBufferName,
		{},
		{ Buffer::Usage::UNIFORM_BUFFER },
		MemoryType::CPU, true);
	
	const Camera& camera = renderInfo.camera->GetComponent<Camera>();
	const Transform& cameraTransform = renderInfo.camera->GetComponent<Transform>();
	const glm::mat4 viewProjectionMat4 = renderInfo.projection * camera.GetViewMat4();
	const glm::mat4 previousViewProjectionMat4 = reflectionBaseMaterial->GetBufferValue<glm::mat4>(globalBuffer, globalBufferName, "camera.viewProjectionMat4");
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.viewProjectionMat4",
		viewProjectionMat4);

	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.previousViewProjectionMat4",
		previousViewProjectionMat4);

	const glm::mat4 viewMat4 = camera.GetViewMat4();
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.viewMat4",
		viewMat4);

	const glm::mat4 inverseViewMat4 = glm::inverse(camera.GetViewMat4());
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.inverseViewMat4",
		inverseViewMat4);

	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.projectionMat4",
		renderInfo.projection);

	const glm::mat4 inverseRotationMat4 = glm::inverse(cameraTransform.GetRotationMat4());
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.inverseRotationMat4",
		inverseRotationMat4);

	{
		const std::array<glm::vec4, 6> frustumPlanes = Utils::GetFrustumPlanes(viewProjectionMat4);
		uint32_t size, offset;
		if (reflectionBaseMaterial->GetUniformDetails(globalBufferName, "camera.frustumPlanes", size, offset))
		{
			globalBuffer->WriteToBuffer((void*)frustumPlanes.data(), size, offset);
		}
	}

	const glm::vec3 positionViewSpace = camera.GetViewMat4() * glm::vec4(cameraTransform.GetPosition(), 1.0f);
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.positionViewSpace",
		positionViewSpace);

	const glm::vec3 positionWorldSpace = cameraTransform.GetPosition();
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.positionWorldSpace",
		positionWorldSpace);

	const float time = Time::GetTime();
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.time",
		time);

	const float deltaTime = Time::GetDeltaTime();
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.deltaTime",
		deltaTime);

	const float zNear = camera.GetZNear();
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.zNear",
		zNear);

	const float zFar = camera.GetZFar();
	reflectionBaseMaterial->WriteToBuffer(
		globalBuffer,
		globalBufferName,
		"camera.zFar",
		zFar);

	const glm::vec2 viewportSize = renderInfo.viewportSize;
	reflectionBaseMaterial->WriteToBuffer(
		renderInfo.renderView->GetBuffer(globalBufferName),
		globalBufferName,
		"camera.viewportSize",
		viewportSize);

	const float aspectRation = viewportSize.x / viewportSize.y;
	reflectionBaseMaterial->WriteToBuffer(
		renderInfo.renderView->GetBuffer(globalBufferName),
		globalBufferName,
		"camera.aspectRatio",
		aspectRation);

	const float tanHalfFOV = tanf(camera.GetFov() / 2.0f);
	reflectionBaseMaterial->WriteToBuffer(
		renderInfo.renderView->GetBuffer(globalBufferName),
		globalBufferName,
		"camera.tanHalfFOV",
		tanHalfFOV);

	const Scene::WindSettings& windSettings = renderInfo.scene->GetWindSettings();
	const glm::vec3 windDirection = glm::normalize(windSettings.direction);
	reflectionBaseMaterial->WriteToBuffer(
		renderInfo.renderView->GetBuffer(globalBufferName),
		globalBufferName,
		"camera.wind.direction",
		windDirection);

	reflectionBaseMaterial->WriteToBuffer(
		renderInfo.renderView->GetBuffer(globalBufferName),
		globalBufferName,
		"camera.wind.strength",
		windSettings.strength);

	reflectionBaseMaterial->WriteToBuffer(
		renderInfo.renderView->GetBuffer(globalBufferName),
		globalBufferName,
		"camera.wind.frequency",
		windSettings.frequency);
}

std::shared_ptr<Texture> RenderPassManager::ScaleTexture(
	std::shared_ptr<Texture> sourceTexture,
	const glm::ivec2& dstSize)
{
	PROFILER_SCOPE(__FUNCTION__);

	const std::shared_ptr<Renderer> renderer = Renderer::Create();
	const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial("Materials/ScaleTexture.basemat");
	const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline("ScaleTexture");
	if (!renderer || !pipeline)
	{
		return nullptr;
	}

	Texture::CreateInfo createInfo{};
	createInfo.aspectMask = Texture::AspectMask::COLOR;
	createInfo.instanceSize = sizeof(uint8_t) * 4;
	createInfo.filepath = "dstScaleTexture";
	createInfo.name = "dstScaleTexture";
	createInfo.format = Format::R8G8B8A8_UNORM;
	createInfo.size = dstSize;
	createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::TRANSFER_SRC };
	createInfo.isMultiBuffered = false;
	std::shared_ptr<Texture> dstTexture = TextureManager::GetInstance().Create(createInfo);

	void* frame = device->Begin();

	glm::uvec2 groupCount = glm::uvec2(dstSize.x / 16, dstSize.y / 16);
	groupCount += glm::uvec2(1, 1);
	renderer->Compute(
		pipeline,
		{ groupCount.x, groupCount.y, 1 },
		{
			dstTexture->GetUniformWriter()->GetNativeHandle(),
			sourceTexture->GetUniformWriter()->GetNativeHandle()
		},
		frame);

	device->End(frame);

	return dstTexture;
}

void RenderPassManager::Initialize()
{
	CreateGBuffer();
	CreateCSM();
	CreatePointLightShadows();
	CreateSpotLightShadows();
	CreateComputeIndirectDrawGBuffer();
	CreateComputeIndirectDrawCSM();
	CreateDeferred();
	CreateAtmosphere();
	CreateTransparent();
	CreateToneMappingPass();
	CreateSSAO();
	CreateSSAOBlur();
	CreateSSS();
	CreateSSSBlur();
	CreateBloom();
	CreateSSR();
	CreateSSRBlur();
	CreateAntiAliasingAndComposePass();
	CreateUI();
	CreateDecalPass();
	CreateDefaultReflection();
	CreateHiZPyramid();
}

void RenderPassManager::GetVertexBuffers(
	std::shared_ptr<Pipeline> pipeline,
	std::shared_ptr<Mesh> mesh,
	std::vector<NativeHandle>& vertexBuffers,
	std::vector<size_t>& vertexBufferOffsets)
{
	PROFILER_SCOPE(__FUNCTION__);

	if (pipeline->GetType() != Pipeline::Type::GRAPHICS)
	{
		FATAL_ERROR("Can't get vertex buffers, pipeline type is not Pipeline::Type::GRAPHICS!");
	}

	vertexBuffers.clear();
	vertexBufferOffsets.clear();

	const std::vector<NativeHandle>& handles = mesh->GetVertexLayoutHandles();
	const std::vector<VertexLayout>& vertexLayouts = mesh->GetVertexLayouts();
	const size_t vertexLayoutCount = vertexLayouts.size();

	const std::shared_ptr<GraphicsPipeline>& graphicsPipeline = std::static_pointer_cast<GraphicsPipeline>(pipeline);
	const auto& bindingDescriptions = graphicsPipeline->GetCreateInfo().bindingDescriptions;

	// TODO: Optimize search.
	for (const auto& bindingDescription : bindingDescriptions)
	{
		if (bindingDescription.inputRate != GraphicsPipeline::InputRate::VERTEX)
		{
			continue;
		}

		for (size_t i = 0; i < vertexLayoutCount; i++)
		{
			if (vertexLayouts[i].tag == bindingDescription.tag)
			{
				vertexBuffers.emplace_back(handles[i]);
				vertexBufferOffsets.emplace_back(0);

				break;
			}
		}
	}
}

void RenderPassManager::ProcessEntities(const RenderPass::RenderCallbackInfo& renderInfo)
{
	const std::shared_ptr<Scene>& scene = renderInfo.scene;

	MultiPassEntityData* multiPassData = (MultiPassEntityData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassEntityData");
	if (!multiPassData)
	{
		multiPassData = new MultiPassEntityData();
		renderInfo.scene->GetRenderView()->SetCustomData("MultiPassEntityData", multiPassData);
	}

	if (!multiPassData->entityUniformWriter)
	{
		BindlessUniformWriter::GetInstance().CreateBindlessEntitiesResources(
			multiPassData->entityUniformWriter,
			multiPassData->entityBuffer);
		renderInfo.scene->GetRenderView()->SetUniformWriter("BindlessEntities", multiPassData->entityUniformWriter);
	}

	void* data = multiPassData->entityBuffer->GetData();
	memset(data, 0, multiPassData->entityBuffer->GetSize());
	multiPassData->entityBuffer->WriteToBuffer(0, 0);

	const std::vector<std::string> renderPasses =
	{
		GBuffer,
		CSM,
		// Add more passes here: PointLightShadows, SpotLightShadows, Transparent, etc.
	};

	multiPassData->totalEntityCount = 0;
	for (const auto& passName : renderPasses)
	{
		multiPassData->passesByName[passName] = MultiPassEntityData::PassData{};
	}

	std::unordered_set<BaseMaterial*> updatedBaseMaterials;

	uint32_t entityIndex = 0;
	scene->GetRegistry().view<Renderer3D>().each([&](auto entity, Renderer3D& r3d)
	{
		// TODO: Put on GPU!
		// if ((r3d.objectVisibilityMask & camera.GetObjectVisibilityMask()) == 0)
		// 	return;

		if (!r3d.material || !r3d.mesh || !r3d.isEnabled)
			return;

		const Transform& transform = scene->GetRegistry().get<Transform>(entity);
		if (!transform.GetEntity()->IsEnabled())
			return;

		BindlessUniformWriter::EntityInfo entityInfo{};
		entityInfo.flags = (uint32_t)BindlessUniformWriter::EntityFlagBits::VALID;
		entityInfo.transform = transform.GetTransform();
		entityInfo.aabb = Utils::LocalToWorldAABB(
			{r3d.mesh->GetBoundingBox().min, r3d.mesh->GetBoundingBox().max},
			entityInfo.transform);
		entityInfo.meshInfoBuffer = r3d.mesh->GetMeshInfoBuffer()->GetDeviceAddress().Get();

		entityInfo.materialInfoBuffer = r3d.material->GetMaterialInfoBuffer()->GetDeviceAddress().Get();

		if (const auto skeletalAnimatorEntity = scene->FindEntityByUUID(r3d.skeletalAnimatorEntityUUID))
		{
			if (auto* skeletalAnimator = scene->GetRegistry().try_get<SkeletalAnimator>(skeletalAnimatorEntity->GetHandle()))
			{
				entityInfo.boneBuffer = skeletalAnimator->GetBuffer()->GetDeviceAddress().Get();
				if (r3d.mesh->GetType() == Mesh::Type::SKINNED)
					entityInfo.flags |= (uint32_t)BindlessUniformWriter::EntityFlagBits::SKINNED;
			}
		}

		BaseMaterial* baseMaterial = r3d.material->GetBaseMaterial().get();
		bool needsBaseMaterialUpdate = updatedBaseMaterials.find(baseMaterial) == updatedBaseMaterials.end();
		if (needsBaseMaterialUpdate)
		{
			BaseMaterial::BaseMaterialInfoBuffer baseMaterialInfo{};
			for (size_t i = 0; i < MAX_PIPELINE_COUNT_PER_MATERIAL; i++)
			{
				baseMaterialInfo.pipelineIds[i] = 0;
			}
			
			for (size_t passIndex = 0; passIndex < renderPasses.size(); ++passIndex)
			{
				const auto& passName = renderPasses[passIndex];
				auto pipeline = baseMaterial->GetPipeline(passName);

				if (!pipeline)
					continue;

				auto& passData = multiPassData->passesByName[passName];
				auto& pipelineInfo = passData.pipelineInfos[pipeline];

				if (pipelineInfo.id == -1)
				{
					pipelineInfo.id = static_cast<int>(passData.sortedPipelines.size());
					passData.sortedPipelines[pipelineInfo.id] = pipeline;
				}

				std::shared_ptr<Pass> pass = RenderPassManager::GetInstance().GetPass(passName);
				if (pass)
				{
					baseMaterialInfo.pipelineIds[pass->GetId()] = pipelineInfo.id;
				}

				pipelineInfo.maxDrawCount++;
				passData.entityCount++;
			}
			
			baseMaterial->GetBaseMaterialInfoBuffer()->WriteToBuffer(
				&baseMaterialInfo,
				sizeof(BaseMaterial::BaseMaterialInfoBuffer),
				0);
			
			updatedBaseMaterials.insert(baseMaterial);
		}
		else
		{
			for (size_t passIndex = 0; passIndex < renderPasses.size(); ++passIndex)
			{
				const auto& passName = renderPasses[passIndex];
				auto pipeline = baseMaterial->GetPipeline(passName);

				if (!pipeline)
					continue;

				auto& passData = multiPassData->passesByName[passName];
				auto& pipelineInfo = passData.pipelineInfos[pipeline];

				pipelineInfo.maxDrawCount++;
				passData.entityCount++;
			}
		}

		multiPassData->entityBuffer->WriteToBuffer(
			&entityInfo,
			sizeof(BindlessUniformWriter::EntityInfo),
			sizeof(BindlessUniformWriter::EntityInfo) * entityIndex++);
	});

	multiPassData->totalEntityCount = entityIndex;

	for (BaseMaterial* baseMaterial : updatedBaseMaterials)
	{
		baseMaterial->GetBaseMaterialInfoBuffer()->Flush();
	}

	multiPassData->entityBuffer->Flush();
}

void RenderPassManager::WriteRenderViews(
	std::shared_ptr<RenderView> cameraRenderView,
	std::shared_ptr<RenderView> sceneRenderView,
	std::shared_ptr<Pipeline> pipeline,
	std::shared_ptr<UniformWriter> uniformWriter)
{
	PROFILER_SCOPE(__FUNCTION__);

	for (const auto& [name, textureAttachmentInfo] : pipeline->GetUniformInfo().textureAttachmentsByName)
	{
		if (cameraRenderView)
		{
			const std::shared_ptr<FrameBuffer> frameBuffer = cameraRenderView->GetFrameBuffer(textureAttachmentInfo.name);
			if (frameBuffer)
			{
				uniformWriter->WriteTextureToFrame(name, frameBuffer->GetAttachment(textureAttachmentInfo.attachmentIndex));
				continue;
			}

			const std::shared_ptr<Texture> texture = cameraRenderView->GetStorageImage(textureAttachmentInfo.name);
			if (texture)
			{
				uniformWriter->WriteTextureToFrame(name, texture);
				continue;
			}
		}

		if (sceneRenderView)
		{
			const std::shared_ptr<FrameBuffer> frameBuffer = sceneRenderView->GetFrameBuffer(textureAttachmentInfo.name);
			if (frameBuffer)
			{
				uniformWriter->WriteTextureToFrame(name, frameBuffer->GetAttachment(textureAttachmentInfo.attachmentIndex));
				continue;
			}

			const std::shared_ptr<Texture> texture = sceneRenderView->GetStorageImage(textureAttachmentInfo.name);
			if (texture)
			{
				uniformWriter->WriteTextureToFrame(name, texture);
				continue;
			}
		}

		if (!textureAttachmentInfo.defaultName.empty())
		{
			uniformWriter->WriteTextureToFrame(name, TextureManager::GetInstance().GetTexture(textureAttachmentInfo.defaultName));
			continue;
		}

		uniformWriter->WriteTextureToFrame(name, TextureManager::GetInstance().GetWhite());
	}
}

std::shared_ptr<UniformWriter> RenderPassManager::GetOrCreateUniformWriter(
	std::shared_ptr<RenderView> renderView,
	std::shared_ptr<Pipeline> pipeline,
	Pipeline::DescriptorSetIndexType descriptorSetIndexType,
	const std::string& uniformWriterName,
	const std::string& uniformWriterIndexByName,
	const bool isMultiBuffered)
{
	PROFILER_SCOPE(__FUNCTION__);

	std::shared_ptr<UniformWriter> renderUniformWriter = renderView->GetUniformWriter(uniformWriterIndexByName.empty() ? uniformWriterName : uniformWriterIndexByName);
	if (!renderUniformWriter)
	{
		const std::shared_ptr<UniformLayout> renderUniformLayout =
			pipeline->GetUniformLayout(*pipeline->GetDescriptorSetIndexByType(descriptorSetIndexType, uniformWriterName));
		renderUniformWriter = UniformWriter::Create(renderUniformLayout, isMultiBuffered);
		renderView->SetUniformWriter(uniformWriterIndexByName.empty() ? uniformWriterName : uniformWriterIndexByName, renderUniformWriter);
	}

	return renderUniformWriter;
}

std::shared_ptr<Buffer> RenderPassManager::GetOrCreateBuffer(
	std::shared_ptr<RenderView> renderView,
	std::shared_ptr<UniformWriter> uniformWriter,
	const std::string& bufferName,
	const std::string& setBufferName,
	const std::vector<Buffer::Usage>& usages,
	const MemoryType memoryType,
	const bool isMultiBuffered)
{
	PROFILER_SCOPE(__FUNCTION__);

	std::shared_ptr<Buffer> buffer = renderView->GetBuffer(setBufferName.empty() ? bufferName : setBufferName);
	if (buffer)
	{
		if (uniformWriter)
		{
			uniformWriter->WriteBufferToAllFrames(bufferName, buffer);
			uniformWriter->Flush();
		}
		return buffer;
	}

	auto binding = uniformWriter->GetUniformLayout()->GetBindingByName(bufferName);
	if (binding && binding->buffer)
	{
		Buffer::CreateInfo createInfo{};
		createInfo.instanceSize = binding->buffer->size;
		createInfo.instanceCount = 1;
		createInfo.usages = usages;
		createInfo.memoryType = memoryType;
		createInfo.isMultiBuffered = isMultiBuffered;
		buffer = Buffer::Create(createInfo);

		renderView->SetBuffer(setBufferName.empty() ? bufferName : setBufferName, buffer);
		uniformWriter->WriteBufferToAllFrames(binding->buffer->name, buffer);
		uniformWriter->Flush();

		return buffer;
	}

	FATAL_ERROR(bufferName + ":Failed to create buffer, no such binding was found!");
}

size_t RenderPassManager::GetLod(
	const glm::vec3& cameraPosition,
	const glm::vec3& meshPosition,
	const float radius,
	const std::vector<Mesh::Lod>& lods)
{
	if (lods.size() == 1)
	{
		return lods.size() - 1;
	}

	const float distance = glm::length(cameraPosition - meshPosition) - radius;

	for (int i = 1; i < lods.size(); ++i)
	{
		if (distance <= lods[i].distanceThreshold)
		{
			return i - 1;
		}
	}

	return lods.size() - 1;
}
