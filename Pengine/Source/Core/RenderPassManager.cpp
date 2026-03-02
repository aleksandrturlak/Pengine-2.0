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
#include "../Graphics/AccelerationStructure.h"
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

	const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, "Camera");
	const std::string cameraBufferName = "CameraBuffer";
	const std::shared_ptr<Buffer> cameraBuffer = GetOrCreateBuffer(
		renderInfo.renderView,
		renderUniformWriter,
		cameraBufferName,
		{},
		{ Buffer::Usage::UNIFORM_BUFFER },
		MemoryType::CPU, true);
	
	const Camera& camera = renderInfo.camera->GetComponent<Camera>();
	const Transform& cameraTransform = renderInfo.camera->GetComponent<Transform>();
	const glm::mat4 viewProjectionMat4 = renderInfo.projection * camera.GetViewMat4();
	const glm::mat4 previousViewProjectionMat4 = reflectionBaseMaterial->GetBufferValue<glm::mat4>(cameraBuffer, cameraBufferName, "camera.viewProjectionMat4");
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.viewProjectionMat4",
		viewProjectionMat4);

	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.previousViewProjectionMat4",
		previousViewProjectionMat4);

	const glm::mat4 viewMat4 = camera.GetViewMat4();
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.viewMat4",
		viewMat4);

	const glm::mat4 inverseViewMat4 = glm::inverse(camera.GetViewMat4());
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.inverseViewMat4",
		inverseViewMat4);

	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.projectionMat4",
		renderInfo.projection);

	const glm::mat4 inverseRotationMat4 = glm::inverse(cameraTransform.GetRotationMat4());
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.inverseRotationMat4",
		inverseRotationMat4);

	{
		const std::array<glm::vec4, 6> frustumPlanes = Utils::GetFrustumPlanes(viewProjectionMat4);
		uint32_t size, offset;
		if (reflectionBaseMaterial->GetUniformDetails(cameraBufferName, "camera.frustumPlanes", size, offset))
		{
			cameraBuffer->WriteToBuffer((void*)frustumPlanes.data(), size, offset);
		}
	}

	const glm::vec3 positionViewSpace = camera.GetViewMat4() * glm::vec4(cameraTransform.GetPosition(), 1.0f);
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.positionViewSpace",
		positionViewSpace);

	const glm::vec3 positionWorldSpace = cameraTransform.GetPosition();
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.positionWorldSpace",
		positionWorldSpace);

	const float time = Time::GetTime();
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.time",
		time);

	const float deltaTime = Time::GetDeltaTime();
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.deltaTime",
		deltaTime);

	const float zNear = camera.GetZNear();
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.zNear",
		zNear);

	const float zFar = camera.GetZFar();
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.zFar",
		zFar);

	const glm::vec2 viewportSize = renderInfo.viewportSize;
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.viewportSize",
		viewportSize);

	const float aspectRation = viewportSize.x / viewportSize.y;
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.aspectRatio",
		aspectRation);

	const float tanHalfFOV = tanf(camera.GetFov() / 2.0f);
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.tanHalfFOV",
		tanHalfFOV);

	const Scene::WindSettings& windSettings = renderInfo.scene->GetWindSettings();
	const glm::vec3 windDirection = glm::normalize(windSettings.direction);
	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.wind.direction",
		windDirection);

	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
		"camera.wind.strength",
		windSettings.strength);

	reflectionBaseMaterial->WriteToBuffer(
		cameraBuffer,
		cameraBufferName,
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
	CreateDecalPass();
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
	CreateRayTracedReflections();
	CreateBlurReflections();
	CreateAntiAliasingAndComposePass();
	CreateUI();
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
		BindlessUniformWriter::GetInstance().CreateSceneResources(
			multiPassData->entityUniformWriter,
			multiPassData->entityBuffer);
		renderInfo.scene->GetRenderView()->SetUniformWriter("Scene", multiPassData->entityUniformWriter);
	}

	void* data = multiPassData->entityBuffer->GetData();
	memset(data, 0, multiPassData->entityBuffer->GetSize());
	multiPassData->entityBuffer->WriteToBuffer(0, 0);

	static const std::string renderPasses[] =
	{
		GBuffer,
		CSM,
		// Add more passes here: PointLightShadows, SpotLightShadows, Transparent, etc.
	};
	constexpr size_t renderPassCount = std::size(renderPasses);

	std::shared_ptr<Pass> cachedPasses[renderPassCount];
	for (size_t i = 0; i < renderPassCount; ++i)
	{
		cachedPasses[i] = RenderPassManager::GetInstance().GetPass(renderPasses[i]);
		multiPassData->passesByName[renderPasses[i]] = MultiPassEntityData::PassData{};
	}

	multiPassData->totalEntityCount = 0;

	std::unordered_set<BaseMaterial*> updatedBaseMaterials;

	const auto view = scene->GetRegistry().view<Renderer3D>();

	std::vector<AccelerationStructure::Instance> tlasInstances;

	uint32_t entityIndex = 0;
	view.each([&](entt::entity entity, Renderer3D& r3d)
	{
		// TODO: Put on GPU!
		// if ((r3d.objectVisibilityMask & camera.GetObjectVisibilityMask()) == 0)
		// 	return;

		r3d.entityIndex = -1;
		r3d.aabb = {};

		if (!r3d.mesh)
			return;

		const Transform& transform = scene->GetRegistry().get<Transform>(entity);
		
		BindlessUniformWriter::EntityInfo entityInfo{};
		entityInfo.transform = transform.GetTransform();

		entityInfo.aabb = r3d.aabb = Utils::LocalToWorldAABB(
			{ r3d.mesh->GetBoundingBox().min, r3d.mesh->GetBoundingBox().max },
			entityInfo.transform);

		if (!r3d.material || !r3d.isEnabled)
			return;

		if (!transform.GetEntity()->IsEnabled())
			return;

		entityInfo.flags = (uint32_t)BindlessUniformWriter::EntityFlagBits::VALID;
		entityInfo.meshInfoBuffer = r3d.mesh->GetMeshInfoBuffer()->GetDeviceAddress().Get();
		entityInfo.materialInfoBuffer = r3d.material->GetMaterialInfoBuffer()->GetDeviceAddress().Get();

		if (r3d.mesh->GetType() == Mesh::Type::SKINNED)
		{
			entityInfo.flags |= (uint32_t)BindlessUniformWriter::EntityFlagBits::SKINNED;
		}
		
		if (r3d.skeletalAnimatorEntityUUID.IsValid())
		{
			if (const auto skeletalAnimatorEntity = scene->FindEntityByUUID(r3d.skeletalAnimatorEntityUUID))
			{
				if (auto* skeletalAnimator = scene->GetRegistry().try_get<SkeletalAnimator>(skeletalAnimatorEntity->GetHandle()))
				{
					entityInfo.boneBuffer = skeletalAnimator->GetBuffer()->GetDeviceAddress().Get();
				}
			}
		}

		BaseMaterial* baseMaterial = r3d.material->GetBaseMaterial().get();

		const auto [_, isNewBaseMaterial] = updatedBaseMaterials.insert(baseMaterial);
		if (isNewBaseMaterial)
		{
			BaseMaterial::BaseMaterialInfoBuffer baseMaterialInfo{};

			for (size_t passIndex = 0; passIndex < renderPassCount; ++passIndex)
			{
				auto pipeline = baseMaterial->GetPipeline(renderPasses[passIndex]);

				if (!pipeline)
					continue;

				auto& passData = multiPassData->passesByName[renderPasses[passIndex]];
				auto& pipelineInfo = passData.pipelineInfos[pipeline];

				if (pipelineInfo.id == -1)
				{
					pipelineInfo.id = static_cast<int>(passData.sortedPipelines.size());
					passData.sortedPipelines[pipelineInfo.id] = pipeline;
				}

				if (cachedPasses[passIndex])
				{
					baseMaterialInfo.pipelineIds[cachedPasses[passIndex]->GetId()] = pipelineInfo.id;
				}

				pipelineInfo.maxDrawCount++;
				passData.entityCount++;
			}

			baseMaterial->GetBaseMaterialInfoBuffer()->WriteToBuffer(
				&baseMaterialInfo,
				sizeof(BaseMaterial::BaseMaterialInfoBuffer),
				0);
		}
		else
		{
			for (size_t passIndex = 0; passIndex < renderPassCount; ++passIndex)
			{
				auto pipeline = baseMaterial->GetPipeline(renderPasses[passIndex]);

				if (!pipeline)
					continue;

				auto& passData = multiPassData->passesByName[renderPasses[passIndex]];
				auto& pipelineInfo = passData.pipelineInfos[pipeline];

				pipelineInfo.maxDrawCount++;
				passData.entityCount++;
			}
		}

		r3d.entityIndex = entityIndex;

		const std::shared_ptr<AccelerationStructure>& blas = r3d.mesh->GetBLAS();
		if (blas)
		{
			const glm::mat4 transformMat4 = glm::transpose(transform.GetTransform());

			AccelerationStructure::Instance instance{};
			memcpy(&instance.transform, glm::value_ptr(transformMat4), sizeof(float[3][4]));
			instance.instanceCustomIndex = r3d.entityIndex;
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.flags = (uint32_t)AccelerationStructure::GeometryInstanceFlagBits::TRIANGLE_FACING_CULL_DISABLE_BIT;
			instance.accelerationStructureReference = blas->GetDeviceAddress();

			tlasInstances.emplace_back(instance);
		}

		multiPassData->entityBuffer->WriteToBuffer(
			&entityInfo,
			sizeof(BindlessUniformWriter::EntityInfo),
			sizeof(BindlessUniformWriter::EntityInfo) * entityIndex++);
	});

	multiPassData->totalEntityCount = entityIndex;

	scene->UpdateTLAS(tlasInstances, renderInfo.frame);

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

void RenderPassManager::ProcessLights(const RenderPass::RenderCallbackInfo& renderInfo)
{
	PROFILER_SCOPE(__FUNCTION__);

	const std::shared_ptr<BaseMaterial> deferredBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
		std::filesystem::path("Materials") / "Deferred.basemat");
	const std::shared_ptr<Pipeline> deferredPipeline = deferredBaseMaterial->GetPipeline(Deferred);
	if (!deferredPipeline)
	{
		return;
	}
	
	MultiPassLightData* multiPassData = (MultiPassLightData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassLightData");
	if (!multiPassData)
	{
		multiPassData = new MultiPassLightData();
		renderInfo.scene->GetRenderView()->SetCustomData("MultiPassLightData", multiPassData);
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

	const Camera& camera = renderInfo.camera->GetComponent<Camera>();
	const GraphicsSettings& graphicsSettings = renderInfo.scene->GetGraphicsSettings();
	entt::registry& registry = renderInfo.scene->GetRegistry();

	deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "brightnessThreshold", graphicsSettings.bloom.brightnessThreshold);

	// Directional light.
	auto directionalLightView = registry.view<DirectionalLight>();
	if (!directionalLightView.empty())
	{
		const entt::entity& entity = directionalLightView.back();
		DirectionalLight& dl = registry.get<DirectionalLight>(entity);
		const Transform& transform = registry.get<Transform>(entity);

		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.color", dl.color);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.intensity", dl.intensity);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.ambient", dl.ambient);

		const int hasRayTracedShadows = graphicsSettings.rayTracing.shadows.directionalLight;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.hasRayTracedShadows", hasRayTracedShadows);

		const glm::vec3 directionWorldSpace = transform.GetForward();
		const glm::vec3 directionViewSpace = glm::normalize(glm::mat3(camera.GetViewMat4()) * directionWorldSpace);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.directionWorldSpace", directionWorldSpace);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.directionViewSpace", directionViewSpace);

		const int hasDirectionalLight = 1;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "hasDirectionalLight", hasDirectionalLight);
	}
	else
	{
		const int hasDirectionalLight = 0;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "hasDirectionalLight", hasDirectionalLight);
	}

	// Point lights.
	{
		const auto pointLightView = registry.view<PointLight>();
		
		std::vector<MultiPassLightData::PointLight>& pointLights = multiPassData->pointLights;
		pointLights.clear();
		pointLights.reserve(pointLightView.size());

		int lightIndex = 0;
		for (const auto& entity : pointLightView)
		{
			if (lightIndex == 32)
				break;

			const Transform& transform = registry.get<Transform>(entity);
			if (!transform.GetEntity()->IsEnabled())
				continue;

			MultiPassLightData::PointLight pointLight{};
			pointLight.entity = entity;
			pointLight.index = lightIndex;

			const PointLight& pl = registry.get<PointLight>(entity);

			const glm::vec3 positionWorldSpace = transform.GetPosition();
			const glm::vec3 positionViewSpace = camera.GetViewMat4() * glm::vec4(positionWorldSpace, 1.0f);
			const int castSSS = pl.castSSS;

			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].positionWorldSpace", lightIndex), positionWorldSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].positionViewSpace", lightIndex), positionViewSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].castSSS", lightIndex), castSSS);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].color", lightIndex), pl.color);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].intensity", lightIndex), pl.intensity);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].radius", lightIndex), pl.radius);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].bias", lightIndex), pl.bias);
			
			auto getPointLightViewMatrix = [](const glm::vec3& position, int faceIndex) -> glm::mat4
			{
				static const glm::vec3 directions[6] =
				{
					glm::vec3(-1.0f, 0.0f,  0.0f),  // -X (Left)
					glm::vec3(1.0f,  0.0f,  0.0f),  // +X (Right)
					glm::vec3(0.0f, -1.0f,  0.0f),  // -Y (Bottom)
					glm::vec3(0.0f,  1.0f,  0.0f),  // +Y (Top)
					glm::vec3(0.0f,  0.0f, -1.0f),  // -Z (Back)
					glm::vec3(0.0f,  0.0f,  1.0f),  // +Z (Front)
				};

				static const glm::vec3 ups[6] =
				{
					glm::vec3(0.0f, -1.0f,  0.0f),  // -X (Left) - Y down for Vulkan
					glm::vec3(0.0f, -1.0f,  0.0f),  // +X (Right) - Y down for Vulkan
					glm::vec3(0.0f,  0.0f, -1.0f),  // -Y (Bottom) - Z down
					glm::vec3(0.0f,  0.0f,  1.0f),  // +Y (Top) - Z up
					glm::vec3(0.0f, -1.0f,  0.0f),  // -Z (Back) - Y down for Vulkan
					glm::vec3(0.0f, -1.0f,  0.0f),  // +Z (Front) - Y down for Vulkan
				};

				assert(faceIndex < 6);

				glm::vec3 target = position + directions[faceIndex];

				return glm::lookAt(position, target, ups[faceIndex]);
			};

			const glm::mat4 projectionMat4 = glm::perspective(
				glm::radians(90.0f),
				1.0f,
				camera.GetZNear(),
				pl.radius);

			for (size_t faceIndex = 0; faceIndex < 6; faceIndex++)
			{
				pointLight.viewProjectionMat4[faceIndex] = projectionMat4 * getPointLightViewMatrix(positionWorldSpace, faceIndex);
				deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].pointLightFaceInfos[{}].viewProjectionMat4", lightIndex, faceIndex), pointLight.viewProjectionMat4[faceIndex]);
			}

			pointLights.emplace_back(pointLight);

			lightIndex++;
		}

		const int pointLightsCount = pointLights.size();
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightsCount", pointLightsCount);

		const GraphicsSettings::Shadows::PointLightShadows& pointLightShadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.pointLightShadows;
		const glm::ivec2 resolutions[4] = { { 1024, 1024 }, { 2048, 2048 }, { 3072, 3072 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapAtlasSize = resolutions[pointLightShadowsSettings.atlasQuality];

		const int faceSizes[4] = { 128, 256, 512, 1024 };
		const int faceSize = faceSizes[pointLightShadowsSettings.faceQuality];

		const int pointLightShadowsEnabled = pointLightShadowsSettings.isEnabled;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightShadows.isEnabled", pointLightShadowsEnabled);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightShadows.shadowMapAtlasSize", shadowMapAtlasSize.x);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightShadows.faceSize", faceSize);
		
		const int isRayTraced = graphicsSettings.rayTracing.shadows.pointLight;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightShadows.isRayTraced", isRayTraced);
	}

	// Spot lights.
	{
		auto spotLightView = registry.view<SpotLight>();

		std::vector<MultiPassLightData::SpotLight>& spotLights = multiPassData->spotLights;
		spotLights.clear();
		spotLights.reserve(spotLightView.size());

		int lightIndex = 0;
		for (const auto& entity : spotLightView)
		{
			if (lightIndex == 32)
				break;

			const Transform& transform = registry.get<Transform>(entity);
			if (!transform.GetEntity()->IsEnabled())
				continue;

			MultiPassLightData::SpotLight spotLight{};
			spotLight.entity = entity;
			spotLight.index = lightIndex;

			const SpotLight& sl = registry.get<SpotLight>(entity);

			const glm::vec3 positionWorldSpace = transform.GetPosition();
			const glm::vec3 positionViewSpace = camera.GetViewMat4() * glm::vec4(positionWorldSpace, 1.0f);
			const glm::vec3 directionViewSpace = glm::mat3(camera.GetViewMat4()) * transform.GetForward();
			const int castSSS = sl.castSSS;

			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].positionWorldSpace", lightIndex), positionWorldSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].positionViewSpace", lightIndex), positionViewSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].directionViewSpace", lightIndex), directionViewSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].castSSS", lightIndex), castSSS);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].color", lightIndex), sl.color);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].intensity", lightIndex), sl.intensity);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].radius", lightIndex), sl.radius);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].bias", lightIndex), sl.bias);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].innerCutOff", lightIndex), sl.innerCutOff);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].outerCutOff", lightIndex), sl.outerCutOff);

			const glm::mat4 viewProjectionMat4 = glm::perspective(
				sl.outerCutOff * 2.0f,
				1.0f,
				camera.GetZNear(),
				sl.radius) * transform.GetInverseTransformMat4();
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("spotLights[{}].viewProjectionMat4", lightIndex), viewProjectionMat4);

			spotLights.emplace_back(spotLight);

			lightIndex++;
		}

		const int spotLightsCount = lightIndex;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightsCount", spotLightsCount);

		const GraphicsSettings::Shadows::SpotLightShadows& spotLightShadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.spotLightShadows;
		const glm::ivec2 resolutions[4] = { { 1024, 1024 }, { 2048, 2048 }, { 3072, 3072 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapAtlasSize = resolutions[spotLightShadowsSettings.atlasQuality];

		const int faceSizes[4] = { 128, 256, 512, 1024 };
		const int faceSize = faceSizes[spotLightShadowsSettings.faceQuality];
		
		const int spotLightShadowsEnabled = spotLightShadowsSettings.isEnabled;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightShadows.isEnabled", spotLightShadowsEnabled);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightShadows.shadowMapAtlasSize", shadowMapAtlasSize.x);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightShadows.faceSize", faceSize);

		const int isRayTraced = graphicsSettings.rayTracing.shadows.spotLight;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "spotLightShadows.isRayTraced", isRayTraced);
	}

	// SSS.
	{
		const GraphicsSettings::Shadows::SSS& sssSettings = graphicsSettings.shadows.sss;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::vec2 viewportScale = glm::vec2(resolutionScales[sssSettings.resolutionScale]);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.maxSteps", sssSettings.maxSteps);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.maxRayDistance", sssSettings.maxRayDistance);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.maxDistance", sssSettings.maxDistance);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.minThickness", sssSettings.minThickness);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.maxThickness", sssSettings.maxThickness);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.viewportScale", viewportScale);
	}
}
