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

void RenderPassManager::CreateComputeIndirectDrawGBuffer()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = ComputeIndirectDrawGBuffer;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(ComputeIndirectDrawGBuffer);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "ComputeIndirectDrawGBuffer.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		MultiPassEntityData* multiPassData = (MultiPassEntityData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassEntityData");
		if (!multiPassData || multiPassData->passesByName.find(GBuffer) == multiPassData->passesByName.end())
		{
			return;
		}

		if (multiPassData->totalEntityCount == 0)
		{
			return;
		}

		const GraphicsSettings::HiZOcclusionCulling& hiZOcclusionCulling = renderInfo.scene->GetGraphicsSettings().hiZOcclusionCulling;

		const auto& gbufferData = multiPassData->passesByName[GBuffer];

		const std::shared_ptr<UniformWriter>& renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, ComputeIndirectDrawGBuffer, {}, true);
		const std::shared_ptr<Buffer>& indirectDrawCommandsBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "IndirectDrawCommands", {}, { Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::GPU, true);
		const std::shared_ptr<Buffer>& indirectDrawCommandCountBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "IndirectDrawCommandCount", {}, { Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::GPU, true);
		const std::shared_ptr<Buffer>& pipelineInfoBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "PipelineInfoBuffer", {}, { Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::CPU, true);
		const std::shared_ptr<Buffer>& hiZPyramidMipLevelCountBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "HiZPyramidMipLevelCountBuffer", {}, { Buffer::Usage::UNIFORM_BUFFER }, MemoryType::CPU, true);

		uint32_t mipLevelCount = 1;
		const std::shared_ptr<Texture> hiZTexture = renderInfo.renderView->GetStorageImage(HiZPyramid);
		if (hiZTexture)
		{
			mipLevelCount = hiZTexture->GetMipLevels();
			std::vector<UniformWriter::TextureInfo> textureInfos(mipLevelCount);
			for (size_t i = 0; i < mipLevelCount; i++)
			{
				UniformWriter::TextureInfo& textureInfo = textureInfos[i];
				textureInfo.texture = hiZTexture;
				textureInfo.baseMipLevel = i;
			}
			
			renderUniformWriter->WriteTexturesToFrame(
				"hiZPyramid",
				textureInfos,
				0,
				(Vk::frameInFlightIndex + 1) % Vk::frameInFlightCount,
				Vk::frameInFlightIndex);
		}
		else
		{
			renderUniformWriter->DeleteTexture("hiZPyramid");
		}

		const int isHiZOcclusionCullingEnabled = hiZOcclusionCulling.isEnabled && hiZTexture;
		baseMaterial->WriteToBuffer(hiZPyramidMipLevelCountBuffer, "HiZPyramidMipLevelCountBuffer", "isHiZOcclusionCullingEnabled", isHiZOcclusionCullingEnabled);
		baseMaterial->WriteToBuffer(hiZPyramidMipLevelCountBuffer, "HiZPyramidMipLevelCountBuffer", "depthBias", hiZOcclusionCulling.depthBias);
		baseMaterial->WriteToBuffer(hiZPyramidMipLevelCountBuffer, "HiZPyramidMipLevelCountBuffer", "mipLevelCount", mipLevelCount);

		uint32_t offset = 0;
		for (const auto& [id, pipeline] : gbufferData.sortedPipelines)
		{
			pipelineInfoBuffer->WriteToBuffer(
				(void*)&offset,
				sizeof(uint32_t),
				sizeof(uint32_t) * id);
			offset += gbufferData.pipelineInfos.at(pipeline).maxDrawCount;
		}
		pipelineInfoBuffer->Flush();

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName,  topLevelRenderPassDebugColor, renderInfo.frame);

			renderInfo.renderer->FillBuffer(
				indirectDrawCommandCountBuffer->GetNativeHandle(),
				indirectDrawCommandCountBuffer->GetSize(),
				0,
				0,
				renderInfo.frame);

			uint32_t groupCount = (multiPassData->totalEntityCount + 15) / 16;
			renderInfo.renderer->Compute(pipeline, { groupCount, 1, 1 }, uniformWriterNativeHandles, renderInfo.frame);
			
			renderInfo.renderer->MemoryBufferBarrierVertexReadWrite(indirectDrawCommandsBuffer->GetNativeHandle(), renderInfo.frame);
			renderInfo.renderer->MemoryBufferBarrierVertexReadWrite(indirectDrawCommandCountBuffer->GetNativeHandle(), renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateComputeIndirectDrawCSM()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = ComputeIndirectDrawCSM;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(ComputeIndirectDrawCSM);

		const GraphicsSettings::Shadows::CSM& shadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.csm;
		if (!shadowsSettings.isEnabled)
		{
			return;
		}

		auto directionalLightView = renderInfo.scene->GetRegistry().view<DirectionalLight>();
		if (directionalLightView.empty())
		{
			return;
		}

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "ComputeIndirectDrawCSM.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		MultiPassEntityData* multiPassData = (MultiPassEntityData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassEntityData");
		if (!multiPassData || multiPassData->passesByName.find(CSM) == multiPassData->passesByName.end())
		{
			return;
		}

		if (multiPassData->totalEntityCount == 0)
		{
			return;
		}

		const auto& csmData = multiPassData->passesByName[CSM];
		if (csmData.entityCount == 0)
		{
			return;
		}

		CSMRenderer* csmRenderer = (CSMRenderer*)renderInfo.renderView->GetCustomData("CSMRenderer");
		if (!csmRenderer)
		{
			csmRenderer = new CSMRenderer();
			renderInfo.renderView->SetCustomData("CSMRenderer", csmRenderer);
		}

		glm::vec3 lightDirection{};
		{
			const entt::entity& entity = directionalLightView.back();
			const Transform& transform = renderInfo.scene->GetRegistry().get<Transform>(entity);
			lightDirection = transform.GetForward();
		}

		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		const Transform& cameraTransform = renderInfo.camera->GetComponent<Transform>();

		const glm::ivec2 resolutions[3] = { { 1024, 1024 }, { 2048, 2048 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapSize = resolutions[shadowsSettings.quality];

		const glm::mat4 projectionMat4 = glm::perspective(
			camera.GetFov(),
			(float)renderInfo.viewportSize.x / (float)renderInfo.viewportSize.y,
			camera.GetZNear(),
			shadowsSettings.maxDistance);

		csmRenderer->GenerateLightSpaceMatrices(
			projectionMat4 * camera.GetViewMat4(),
			lightDirection,
			shadowMapSize,
			camera.GetZNear(),
			shadowsSettings.maxDistance,
			shadowsSettings.cascadeCount,
			shadowsSettings.splitFactor,
			shadowsSettings.stabilizeCascades);

		if (csmRenderer->GetLightSpaceMatrices().empty())
		{
			return;
		}

		const std::shared_ptr<UniformWriter>& lightSpaceUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, ComputeIndirectDrawCSM, {}, true);
		
		const std::string lightSpaceMatricesBufferName = "LightSpaceMatrices";
		const std::shared_ptr<Buffer> lightSpaceMatricesBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			lightSpaceUniformWriter,
			lightSpaceMatricesBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU, true);

		baseMaterial->WriteToBuffer(
			lightSpaceMatricesBuffer,
			lightSpaceMatricesBufferName,
			"lightSpaceMatrices",
			*csmRenderer->GetLightSpaceMatrices().data());

		const int cascadeCount = csmRenderer->GetLightSpaceMatrices().size();
		baseMaterial->WriteToBuffer(
			lightSpaceMatricesBuffer,
			lightSpaceMatricesBufferName,
			"cascadeCount",
			cascadeCount);
		lightSpaceMatricesBuffer->Flush();

		const std::shared_ptr<UniformWriter>& renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, "ComputeIndirectDrawCSMBuffers", {}, true);
		const std::shared_ptr<Buffer>& indirectDrawCommandsBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "IndirectDrawCommands", "IndirectDrawCommandsCSM", 
			{ Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::GPU, true);
		const std::shared_ptr<Buffer>& indirectDrawCommandCountBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "IndirectDrawCommandCount", "IndirectDrawCommandCountCSM", 
			{ Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::GPU, true);
		const std::shared_ptr<Buffer>& pipelineInfoBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "PipelineInfoBuffer", "PipelineInfoBufferCSM", 
			{ Buffer::Usage::STORAGE_BUFFER, Buffer::Usage::INDIRECT_BUFFER }, MemoryType::CPU, true);
		const std::shared_ptr<Buffer>& csmInstanceDataBuffer = GetOrCreateBuffer(
			renderInfo.renderView, renderUniformWriter, "CSMInstanceDataBuffer", "CSMInstanceDataBufferCSM", 
			{ Buffer::Usage::STORAGE_BUFFER }, MemoryType::GPU, true);

		uint32_t pipelineOffset = 0;
		for (int cascade = 0; cascade < cascadeCount; ++cascade)
		{
			for (const auto& [id, pipelinePtr] : csmData.sortedPipelines)
			{
				uint32_t cascadePipelineIndex = cascade * MAX_PIPELINE_COUNT + id;
				pipelineInfoBuffer->WriteToBuffer(
					(void*)&pipelineOffset,
					sizeof(uint32_t),
					sizeof(uint32_t) * cascadePipelineIndex);
				pipelineOffset += csmData.pipelineInfos.at(pipelinePtr).maxDrawCount;
			}
		}
		pipelineInfoBuffer->Flush();

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			renderInfo.renderer->FillBuffer(
				indirectDrawCommandCountBuffer->GetNativeHandle(),
				indirectDrawCommandCountBuffer->GetSize(),
				0,
				0,
				renderInfo.frame);

			uint32_t groupCount = (multiPassData->totalEntityCount + 15) / 16;
			renderInfo.renderer->Compute(pipeline, { groupCount, 1, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->MemoryBufferBarrierVertexReadWrite(indirectDrawCommandsBuffer->GetNativeHandle(), renderInfo.frame);
			renderInfo.renderer->MemoryBufferBarrierVertexReadWrite(indirectDrawCommandCountBuffer->GetNativeHandle(), renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateGBuffer()
{
	RenderPass::ClearDepth clearDepth{};
	clearDepth.clearDepth = 0.0f;
	clearDepth.clearStencil = 0;

	glm::vec4 clearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
	glm::vec4 clearNormal = { 0.0f, 0.0f, 0.0f, 0.0f };
	glm::vec4 clearShading = { 0.0f, 0.0f, 0.0f, 0.0f };
	glm::vec4 clearEmissive = { 0.0f, 0.0f, 0.0f, 0.0f };

	Texture::SamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER;
	samplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_BLACK;
	samplerCreateInfo.maxAnisotropy = 1.0f;

	RenderPass::AttachmentDescription color{};
	color.textureCreateInfo.format = Format::B10G11R11_UFLOAT_PACK32;
	color.textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
	color.textureCreateInfo.instanceSize = sizeof(uint32_t);
	color.textureCreateInfo.isMultiBuffered = true;
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
	normal.textureCreateInfo.isMultiBuffered = true;
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
	shading.textureCreateInfo.isMultiBuffered = true;
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
	emissive.textureCreateInfo.isMultiBuffered = true;
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
	depth.textureCreateInfo.isMultiBuffered = true;
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

void RenderPassManager::CreateDeferred()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = Deferred;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(Deferred);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "Deferred.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const std::string lightsBufferName = "Lights";
		const std::shared_ptr<UniformWriter> lightsUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, lightsBufferName);
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
		baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "brightnessThreshold", graphicsSettings.bloom.brightnessThreshold);

		auto directionalLightView = renderInfo.scene->GetRegistry().view<DirectionalLight>();
		if (!directionalLightView.empty())
		{
			const entt::entity& entity = directionalLightView.back();
			DirectionalLight& dl = renderInfo.scene->GetRegistry().get<DirectionalLight>(entity);
			const Transform& transform = renderInfo.scene->GetRegistry().get<Transform>(entity);

			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.color", dl.color);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.intensity", dl.intensity);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.ambient", dl.ambient);

			const glm::vec3 directionWorldSpace = transform.GetForward();
			const glm::vec3 directionViewSpace = glm::normalize(glm::mat3(camera.GetViewMat4()) * directionWorldSpace);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.directionWorldSpace", directionWorldSpace);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "directionalLight.directionViewSpace", directionViewSpace);

			const int hasDirectionalLight = 1;
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "hasDirectionalLight", hasDirectionalLight);

			const GraphicsSettings::Shadows::CSM& shadowSettings = renderInfo.scene->GetGraphicsSettings().shadows.csm;

			const int isEnabled = shadowSettings.isEnabled;
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.isEnabled", isEnabled);

			CSMRenderer* csmRenderer = (CSMRenderer*)renderInfo.renderView->GetCustomData("CSMRenderer");
			if (shadowSettings.isEnabled && !csmRenderer->GetLightSpaceMatrices().empty())
			{
				// Also in Shaders/Includes/CSM.h
				constexpr size_t maxCascadeCount = 10;

				std::vector<glm::vec4> shadowCascadeLevels(maxCascadeCount, glm::vec4{});
				for (size_t i = 0; i < csmRenderer->GetDistances().size(); i++)
				{
					shadowCascadeLevels[i] = glm::vec4(csmRenderer->GetDistances()[i]);
				}

				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.lightSpaceMatrices", *csmRenderer->GetLightSpaceMatrices().data());

				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.distances", *shadowCascadeLevels.data());

				const int cascadeCount = csmRenderer->GetLightSpaceMatrices().size();
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.cascadeCount", cascadeCount);

				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.fogFactor", shadowSettings.fogFactor);
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.maxDistance", shadowSettings.maxDistance);
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.pcfRange", shadowSettings.pcfRange);

				const int filter = (int)shadowSettings.filter;
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.filtering", filter);

				const int visualize = shadowSettings.visualize;
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.visualize", visualize);

				std::vector<glm::vec4> biases(maxCascadeCount);
				for (size_t i = 0; i < shadowSettings.biases.size(); i++)
				{
					biases[i] = glm::vec4(shadowSettings.biases[i]);
				}
				baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.biases", *biases.data());
			}
		}
		else
		{
			const int hasDirectionalLight = 0;
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "hasDirectionalLight", hasDirectionalLight);
			baseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "csm.cascadeCount", hasDirectionalLight);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		const std::shared_ptr<Texture> colorTexture = renderInfo.renderView->GetFrameBuffer(Transparent)->GetAttachment(0);
		const std::shared_ptr<Texture> emissiveTexture = renderInfo.renderView->GetFrameBuffer(GBuffer)->GetAttachment(3);

		const std::shared_ptr<UniformWriter> outputUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, "DeferredOutput");
		outputUniformWriter->WriteTextureToFrame("outColor", colorTexture);
		outputUniformWriter->WriteTextureToFrame("outEmissive", emissiveTexture);
		
		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			glm::uvec2 groupCount = renderInfo.viewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
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

void RenderPassManager::CreateCSM()
{
	RenderPass::ClearDepth clearDepth{};
	clearDepth.clearDepth = 1.0f;
	clearDepth.clearStencil = 0;

	RenderPass::AttachmentDescription depth{};
	depth.textureCreateInfo.format = Format::D32_SFLOAT;
	depth.textureCreateInfo.aspectMask = Texture::AspectMask::DEPTH;
	depth.textureCreateInfo.instanceSize = sizeof(float);
	depth.textureCreateInfo.isMultiBuffered = true;
	depth.textureCreateInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::DEPTH_STENCIL_ATTACHMENT };
	depth.textureCreateInfo.name = "CSM";
	depth.textureCreateInfo.filepath = depth.textureCreateInfo.name;
	depth.layout = Texture::Layout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Texture::SamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.filter = Texture::SamplerCreateInfo::Filter::LINEAR;
	samplerCreateInfo.borderColor = Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_WHITE;
	samplerCreateInfo.addressMode = Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_EDGE;
	samplerCreateInfo.maxAnisotropy = 1.0f;

	depth.textureCreateInfo.samplerCreateInfo = samplerCreateInfo;

	RenderPass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::GRAPHICS;
	createInfo.name = CSM;
	createInfo.clearDepths = { clearDepth };
	createInfo.attachmentDescriptions = { depth };
	createInfo.resizeWithViewport = false;
	createInfo.createFrameBuffer = false;

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(CSM);
		const std::string& renderPassName = renderInfo.renderPass->GetName();

		const GraphicsSettings::Shadows::CSM& shadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.csm;
		if (!shadowsSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(renderPassName);
			renderInfo.renderView->DeleteCustomData("CSMRenderer");
			renderInfo.renderView->DeleteBuffer("LightSpaceMatrices");
			renderInfo.renderView->DeleteFrameBuffer(renderPassName);

			return;
		}

		CSMRenderer* csmRenderer = (CSMRenderer*)renderInfo.renderView->GetCustomData("CSMRenderer");
		if (!csmRenderer)
		{
			csmRenderer = new CSMRenderer();

			renderInfo.renderView->SetCustomData("CSMRenderer", csmRenderer);
		}

		const glm::ivec2 resolutions[3] = { { 1024, 1024 }, { 2048, 2048 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapSize = resolutions[shadowsSettings.quality];

		std::shared_ptr<FrameBuffer> frameBuffer = renderInfo.renderView->GetFrameBuffer(renderPassName);
		if (!frameBuffer)
		{
			const std::string& renderPassName = renderInfo.renderPass->GetName();
			renderInfo.renderPass->GetAttachmentDescriptions().back().textureCreateInfo.layerCount = shadowsSettings.cascadeCount;
			frameBuffer = FrameBuffer::Create(renderInfo.renderPass, renderInfo.renderView.get(), shadowMapSize);

			renderInfo.renderView->SetFrameBuffer(renderPassName, frameBuffer);
		}

		const uint32_t currentLayerCount = frameBuffer->GetAttachmentCreateInfos().back().layerCount;
		const bool needsLayerCountUpdate = currentLayerCount != static_cast<uint32_t>(shadowsSettings.cascadeCount);
		
		if (frameBuffer->GetSize() != shadowMapSize || needsLayerCountUpdate)
		{
			frameBuffer->GetAttachmentCreateInfos().back().layerCount = shadowsSettings.cascadeCount;
			frameBuffer->Resize(shadowMapSize);
		}

		const std::shared_ptr<Scene> scene = renderInfo.scene;
		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		entt::registry& registry = scene->GetRegistry();

		glm::vec3 lightDirection{};
		auto directionalLightView = renderInfo.scene->GetRegistry().view<DirectionalLight>();
		if (directionalLightView.empty())
		{
			return;
		}

		{
			const entt::entity& entity = directionalLightView.back();
			DirectionalLight& dl = renderInfo.scene->GetRegistry().get<DirectionalLight>(entity);
			const Transform& transform = renderInfo.scene->GetRegistry().get<Transform>(entity);
			lightDirection = transform.GetForward();
		}

		const glm::mat4 projectionMat4 = glm::perspective(
			camera.GetFov(),
			(float)renderInfo.viewportSize.x / (float)renderInfo.viewportSize.y,
			camera.GetZNear(),
			shadowsSettings.maxDistance);

		csmRenderer->GenerateLightSpaceMatrices(
			projectionMat4 * camera.GetViewMat4(),
			lightDirection,
			shadowMapSize,
			camera.GetZNear(),
			shadowsSettings.maxDistance,
			shadowsSettings.cascadeCount,
			shadowsSettings.splitFactor,
			shadowsSettings.stabilizeCascades);

		MultiPassEntityData* multiPassData = (MultiPassEntityData*)renderInfo.scene->GetRenderView()->GetCustomData("MultiPassEntityData");

		RenderPass::SubmitInfo submitInfo{};
		submitInfo.frame = renderInfo.frame;
		submitInfo.renderPass = renderInfo.renderPass;
		submitInfo.frameBuffer = frameBuffer;

		const int cascadeCount = static_cast<int>(csmRenderer->GetLightSpaceMatrices().size());

		if (multiPassData && multiPassData->passesByName.find(CSM) != multiPassData->passesByName.end())
		{
			const auto& csmData = multiPassData->passesByName[CSM];

			const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
				std::filesystem::path("Materials") / "MeshBase.basemat");
			if (!baseMaterial)
			{
				renderInfo.renderer->BeginRenderPass(submitInfo);
				renderInfo.renderer->EndRenderPass(submitInfo);
				return;
			}

			std::shared_ptr<Buffer> lightSpaceMatricesBuffer;

			renderInfo.renderer->BeginRenderPass(submitInfo);

			const std::shared_ptr<Buffer>& csmInstanceDataBuffer = GetOrCreateBuffer(
				renderInfo.renderView, nullptr, "CSMInstanceDataBufferCSM");
			const std::shared_ptr<Buffer>& indirectDrawCommandsBuffer = GetOrCreateBuffer(
				renderInfo.renderView, nullptr, "IndirectDrawCommandsCSM");
			const std::shared_ptr<Buffer>& indirectDrawCommandCountBuffer = GetOrCreateBuffer(
				renderInfo.renderView, nullptr, "IndirectDrawCommandCountCSM");
			const std::shared_ptr<Buffer>& pipelineInfoBuffer = GetOrCreateBuffer(
				renderInfo.renderView, nullptr, "PipelineInfoBufferCSM");

			for (int cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
			{
				renderInfo.renderer->BeginCommandLabel(std::format("Cascade {}", cascadeIndex), glm::vec3(1.0f, 1.0f, 0.0f), renderInfo.frame);
				
				for (const auto& [id, pipeline] : csmData.sortedPipelines)
				{
					renderInfo.renderer->BindPipeline(pipeline, renderInfo.frame);

					const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
						renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, renderPassName);
					const std::string lightSpaceMatricesBufferName = "LightSpaceMatrices";
					lightSpaceMatricesBuffer = GetOrCreateBuffer(
						renderInfo.renderView,
						renderUniformWriter,
						lightSpaceMatricesBufferName,
						{},
						{ Buffer::Usage::UNIFORM_BUFFER },
						MemoryType::CPU,
						true);

					if (cascadeIndex == 0)
					{
						baseMaterial->WriteToBuffer(
							lightSpaceMatricesBuffer,
							lightSpaceMatricesBufferName,
							"lightSpaceMatrices",
							*csmRenderer->GetLightSpaceMatrices().data());

						baseMaterial->WriteToBuffer(
							lightSpaceMatricesBuffer,
							lightSpaceMatricesBufferName,
							"cascadeCount",
							cascadeCount);
						lightSpaceMatricesBuffer->Flush();

						renderUniformWriter->WriteBufferToFrame("CSMInstanceDataBuffer", csmInstanceDataBuffer);
					}

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
						uint32_t cascadePipelineIndex = cascadeIndex * MAX_PIPELINE_COUNT + id;
						uint32_t commandOffset = reinterpret_cast<uint32_t*>(pipelineInfoBuffer->GetData())[cascadePipelineIndex];

						renderInfo.renderer->DrawIndirectCount(
							indirectDrawCommandsBuffer->GetNativeHandle(),
							commandOffset,
							indirectDrawCommandCountBuffer->GetNativeHandle(),
							cascadePipelineIndex,
							csmData.pipelineInfos.at(pipeline).maxDrawCount,
							renderInfo.frame);
					}
				}

				renderInfo.renderer->EndCommandLabel(renderInfo.frame);
			}

			renderInfo.renderer->EndRenderPass(submitInfo);
		}
		else
		{
			renderInfo.renderer->BeginRenderPass(submitInfo);
			renderInfo.renderer->EndRenderPass(submitInfo);
		}
	};

	CreateRenderPass(createInfo);
}

void RenderPassManager::CreatePointLightShadows()
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
	depth.textureCreateInfo.name = "PointLightShadows";
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
	createInfo.name = PointLightShadows;
	createInfo.clearDepths = { clearDepth };
	createInfo.attachmentDescriptions = { depth };
	createInfo.resizeWithViewport = false;
	createInfo.createFrameBuffer = false;

	createInfo.executeCallback = [this](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(PointLightShadows);

		const std::string& renderPassName = renderInfo.renderPass->GetName();

		size_t renderableCount = 0;
		const std::shared_ptr<Scene> scene = renderInfo.scene;
		const Camera& camera = renderInfo.camera->GetComponent<Camera>();
		const glm::vec3 cameraPosition = camera.GetEntity()->GetComponent<Transform>().GetPosition();
		entt::registry& registry = scene->GetRegistry();

		struct FaceInfo
		{
			std::vector<entt::entity> entities;
			RenderableEntities renderableEntities;
			int faceIndex = -1;
		};

		struct LightInfo
		{
			std::array<FaceInfo, 6> faceInfos;
			int lightIndex = -1;
			int shadowMapIndex = -1;
		};

		std::vector<LightInfo> lightInfos;

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

		const GraphicsSettings::Shadows::PointLightShadows& pointLightShadowsSettings = renderInfo.scene->GetGraphicsSettings().shadows.pointLightShadows;
		const glm::ivec2 resolutions[4] = { { 1024, 1024 }, { 2048, 2048 }, { 3072, 3072 }, { 4096, 4096 } };
		const glm::ivec2 shadowMapAtlasSize = resolutions[pointLightShadowsSettings.atlasQuality];

		const int faceSizes[4] = { 128, 256, 512, 1024 };
		const int faceSize = faceSizes[pointLightShadowsSettings.faceQuality];

		const float facesPerRow = (float)shadowMapAtlasSize.x / (float)faceSize;
		const int maxShadowMapCount = glm::floor((facesPerRow * facesPerRow) / 6.0f);

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

		const int isPointLightShadowsEnabled = pointLightShadowsSettings.isEnabled;
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightShadows.isEnabled", isPointLightShadowsEnabled);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightShadows.shadowMapAtlasSize", shadowMapAtlasSize.x);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightShadows.faceSize", faceSize);

		const auto view = registry.view<PointLight>();

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
			lightInfoToSort.radius = registry.get<PointLight>(view[i]).radius;

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

			PointLight& pl = registry.get<PointLight>(light.entity);

			const glm::vec3 lightPositionWorldSpace = light.position;
			const glm::vec3 lightPositionViewSpace = camera.GetViewMat4() * glm::vec4(light.position, 1.0f);
			const int castSSS = pl.castSSS;
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].positionWorldSpace", lightIndex), lightPositionWorldSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].positionViewSpace", lightIndex), lightPositionViewSpace);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].castSSS", lightIndex), castSSS);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].color", lightIndex), pl.color);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].intensity", lightIndex), pl.intensity);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].radius", lightIndex), pl.radius);
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].bias", lightIndex), pl.bias);

			if (pl.drawBoundingSphere)
			{
				constexpr glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f);
				Transform& transform = registry.get<Transform>(light.entity);
				renderInfo.scene->GetVisualizer().DrawSphere(color, transform.GetTransform(), pl.radius, 10);
			}

			int plShadowMapIndex = -1;
			if (pointLightShadowsSettings.isEnabled && pl.castShadows)
			{
				if (shadowMapIndex < maxShadowMapCount)
				{
					plShadowMapIndex = shadowMapIndex;
					shadowMapIndex++;
				}

				const glm::mat4 projectionMat4 = glm::perspective(
					glm::radians(90.0f),
					1.0f,
					camera.GetZNear(),
					pl.radius);

				std::vector<SceneBVH::BVHNode> bvhNodes;
				scene->GetBVH()->Traverse([&light, &bvhNodes](const SceneBVH::BVHNode& node)
				{
					if (!Utils::IntersectAABBvsSphere(node.aabb.min, node.aabb.max, light.position, light.radius))
					{
						return false;
					}

					if (node.IsLeaf() && node.entity->IsValid())
					{
						bvhNodes.emplace_back(node);
					}

					return true;
				});

				for (size_t faceIndex = 0; faceIndex < 6; faceIndex++)
				{
					const glm::mat4 viewProjectionMat4 = projectionMat4 * getPointLightViewMatrix(lightPositionWorldSpace, faceIndex);

					FaceInfo& faceInfo = lightInfo.faceInfos[faceIndex];
					faceInfo.faceIndex = faceIndex;
					faceInfo.entities.reserve(bvhNodes.size());

					const auto frustumPlanes = Utils::GetFrustumPlanes(viewProjectionMat4);
					
					// NOTE: BVH Culling is a lot slower than this for loop checks.
					for (const auto& node : bvhNodes)
					{
						if (Utils::isAABBInsideFrustum(frustumPlanes, node.aabb.min, node.aabb.max))
						{
							faceInfo.entities.emplace_back(node.entity->GetHandle());
						}
					}

					deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].pointLightFaceInfos[{}].viewProjectionMat4", lightIndex, faceIndex), viewProjectionMat4);
				}
			}
			
			lightInfo.shadowMapIndex = plShadowMapIndex;
			deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, std::format("pointLights[{}].shadowMapIndex", lightIndex), plShadowMapIndex);

			lightIndex++;
		}

		int pointLightsCount = lightInfos.size();
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "pointLightsCount", pointLightsCount);

		if (!pointLightShadowsSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(renderPassName);
			renderInfo.renderView->DeleteBuffer("InstanceBufferPointLightShadows");
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
			for (FaceInfo& faceInfo : lightInfo.faceInfos)
			{
				for (const entt::entity& entity : faceInfo.entities)
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

					// 	faceInfo.renderableEntities[r3d.material->GetBaseMaterial()][r3d.material].single.emplace_back(single);
					// }
					// else if (r3d.mesh->GetType() == Mesh::Type::STATIC)
					{
						auto& entities = faceInfo.renderableEntities[r3d.material->GetBaseMaterial()][r3d.material].instanced[r3d.mesh];
						entities.resize(r3d.mesh->GetLods().size());
						entities[lod].reserve(50);
						entities[lod].emplace_back(entity);
					}

					renderableCount++;
				}
			}
		}

		struct InstanceData
		{
			glm::mat4 transform;
			int lightIndex;
			int faceIndex;
		};

		std::shared_ptr<Buffer> instanceBuffer = renderInfo.renderView->GetBuffer("InstanceBufferPointLightShadows");
		if ((renderableCount != 0 && !instanceBuffer) || (instanceBuffer && renderableCount != 0 && instanceBuffer->GetInstanceCount() < renderableCount))
		{
			Buffer::CreateInfo createInfo{};
			createInfo.instanceSize = sizeof(InstanceData);
			createInfo.instanceCount = renderableCount * 2;
			createInfo.usages = { Buffer::Usage::VERTEX_BUFFER };
			createInfo.memoryType = MemoryType::CPU;
			createInfo.isMultiBuffered = true;
			instanceBuffer = Buffer::Create(createInfo);

			renderInfo.renderView->SetBuffer("InstanceBufferPointLightShadows", instanceBuffer);
		}

		struct ShadowMapViewportInfo
		{
			uint32_t textureWidth;
			uint32_t textureHeight;
			uint32_t faceSize;
		};

		auto getShadowMapFaceViewport = [](
			const ShadowMapViewportInfo& info,
			uint32_t shadowMapIndex,
			uint32_t faceIndex)
		{
			RenderPass::Viewport viewport{};

			uint32_t totalIndex = shadowMapIndex * 6 + faceIndex;

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
			uint32_t shadowMapIndex,
			uint32_t faceIndex)
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
		
		renderInfo.renderer->BeginCommandLabel(PointLightShadows, topLevelRenderPassDebugColor, renderInfo.frame);
		
		renderInfo.renderer->BeginCommandLabel("ClearPointLightShadowMapAtlas", { 1.0f, 1.0f, 0.0f }, renderInfo.frame);
		{
			RenderPass::ClearDepth clearDepth{};
			clearDepth.clearDepth = 1.0f;
			clearDepth.clearStencil = 0;
			renderInfo.renderer->ClearDepthStencilImage(frameBuffer->GetAttachment(0), clearDepth, renderInfo.frame);
		}
		renderInfo.renderer->EndCommandLabel(renderInfo.frame);

		for (const LightInfo& lightInfo : lightInfos)
		{
			renderInfo.renderer->BeginCommandLabel("PointLight", { 1.0f, 1.0f, 0.0f }, renderInfo.frame);
			for (const FaceInfo& faceInfo : lightInfo.faceInfos)
			{
				RenderPass::SubmitInfo submitInfo{};
				submitInfo.frame = renderInfo.frame;
				submitInfo.renderPass = renderInfo.renderPass;
				submitInfo.frameBuffer = frameBuffer;
				submitInfo.viewport = getShadowMapFaceViewport(shadowMapViewportInfo, lightInfo.shadowMapIndex, faceInfo.faceIndex);
				submitInfo.scissors = getShadowMapFaceScissor(*submitInfo.viewport, lightInfo.shadowMapIndex, faceInfo.faceIndex);
				renderInfo.renderer->BeginRenderPass(submitInfo, std::format("Face {}", faceInfo.faceIndex), { 1.0f, 1.0f, 0.0f });

				// Render all base materials -> materials -> meshes | put gameobjects into the instance buffer.
				for (const auto& [baseMaterial, meshesByMaterial] : faceInfo.renderableEntities)
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
									data.faceIndex = faceInfo.faceIndex;
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
							data.faceIndex = faceInfo.faceIndex;
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
							// 	newUniformWriterNativeHandles.emplace_back(skeletalAnimator->GetUniformWriter()->GetNativeHandle());

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

void RenderPassManager::CreateSSR()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSR;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSR);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSR.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::SSR& ssrSettings = renderInfo.scene->GetGraphicsSettings().ssr;
		const std::string ssrBufferName = passName;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[ssrSettings.resolutionScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t) * 4;
		createInfo.filepath = passName;
		createInfo.name = passName;
		createInfo.format = Format::R8G8B8A8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = true;

		std::shared_ptr<Texture> ssrTexture = renderInfo.renderView->GetStorageImage(passName);

		if (ssrSettings.isEnabled)
		{
			if (!ssrTexture)
			{
				ssrTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage(passName, ssrTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView,
					pipeline,
					Pipeline::DescriptorSetIndexType::RENDERER,
					passName)->WriteTextureToAllFrames("outColor", ssrTexture);
			}
		}
		else
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage(passName);
			renderInfo.renderView->DeleteBuffer(ssrBufferName);

			return;
		}

		if (currentViewportSize != ssrTexture->GetSize())
		{
			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
			const std::shared_ptr<Texture> ssrTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage(passName, ssrTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", ssrTexture);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);
		if (const auto frameBuffer = renderInfo.scene->GetRenderView()->GetFrameBuffer(Atmosphere))
		{
			renderUniformWriter->WriteTextureToFrame("skyboxTexture", frameBuffer->GetAttachment(0));
		}

		const glm::vec2 viewportScale = glm::vec2(resolutionScales[ssrSettings.resolutionScale]);
		const int useSkyBoxFallback = ssrSettings.useSkyBoxFallback;
		const std::shared_ptr<Buffer> ssrBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			ssrBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "viewportScale", viewportScale);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "maxDistance", ssrSettings.maxDistance);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "resolution", ssrSettings.resolution);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "stepCount", ssrSettings.stepCount);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "thickness", ssrSettings.thickness);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "useSkyBoxFallback", useSkyBoxFallback);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateSSRBlur()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSRBlur;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSRBlur);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSRBlur.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::SSR& ssrSettings = renderInfo.scene->GetGraphicsSettings().ssr;
		const std::string ssrBufferName = passName;
		constexpr float resolutionScales[] = { 0.125f, 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[ssrSettings.resolutionBlurScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t) * 4;
		createInfo.filepath = passName;
		createInfo.name = passName;
		createInfo.format = Format::R8G8B8A8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::TRANSFER_DST };
		createInfo.isMultiBuffered = true;
		createInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(
			createInfo.size.x, createInfo.size.y)))) + 1;

		std::shared_ptr<Texture> ssrBlurTexture = renderInfo.renderView->GetStorageImage(passName);

		if (ssrSettings.isEnabled)
		{
			if (!ssrBlurTexture)
			{
				ssrBlurTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage(passName, ssrBlurTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView,
					pipeline,
					Pipeline::DescriptorSetIndexType::RENDERER,
					passName)->WriteTextureToAllFrames("outColor", ssrBlurTexture);
			}
		}
		else
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage(passName);
			renderInfo.renderView->DeleteBuffer(ssrBufferName);

			return;
		}

		if (currentViewportSize != ssrBlurTexture->GetSize())
		{
			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
			const std::shared_ptr<Texture> ssrBlurTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage(passName, ssrBlurTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", ssrBlurTexture);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		const int blur = (int)ssrSettings.blur;

		const std::shared_ptr<Buffer> ssrBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			ssrBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "blur", blur);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "blurRange", ssrSettings.blurRange);
		baseMaterial->WriteToBuffer(ssrBuffer, ssrBufferName, "blurOffset", ssrSettings.blurOffset);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->MemoryBarrierFragmentReadWrite(renderInfo.frame);

			ssrBlurTexture->GenerateMipMaps(renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateSSAO()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSAO;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSAO);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSAO.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::SSAO& ssaoSettings = renderInfo.scene->GetGraphicsSettings().ssao;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[ssaoSettings.resolutionScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t);
		createInfo.filepath = passName;
		createInfo.name = passName;
		createInfo.format = Format::R8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = true;

		SSAORenderer* ssaoRenderer = (SSAORenderer*)renderInfo.renderView->GetCustomData("SSAORenderer");
		if (ssaoSettings.isEnabled && !ssaoRenderer)
		{
			ssaoRenderer = new SSAORenderer();
			renderInfo.renderView->SetCustomData("SSAORenderer", ssaoRenderer);
		}

		const std::string ssaoBufferName = passName;
		std::shared_ptr<Texture> ssaoTexture = renderInfo.renderView->GetStorageImage(passName);

		if (!ssaoSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteCustomData("SSAORenderer");
			renderInfo.renderView->DeleteBuffer(ssaoBufferName);
			renderInfo.renderView->DeleteStorageImage(passName);

			return;
		}
		else
		{
			if (!renderInfo.renderView->GetStorageImage(passName))
			{
				ssaoTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage(passName, ssaoTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView,
					pipeline,
					Pipeline::DescriptorSetIndexType::RENDERER,
					passName)->WriteTextureToAllFrames("outColor", ssaoTexture);
			}
		}

		if (currentViewportSize != ssaoTexture->GetSize())
		{
			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
			const std::shared_ptr<Texture> ssaoTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage(passName, ssaoTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", ssaoTexture);
		}

		if (ssaoRenderer->GetKernelSize() != ssaoSettings.kernelSize)
		{
			ssaoRenderer->GenerateSamples(ssaoSettings.kernelSize);
		}
		if (ssaoRenderer->GetNoiseSize() != ssaoSettings.noiseSize)
		{
			ssaoRenderer->GenerateNoiseTexture(ssaoSettings.noiseSize);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		const std::shared_ptr<Buffer> ssaoBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			ssaoBufferName,
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);
		renderUniformWriter->WriteTextureToFrame("noiseTexture", ssaoRenderer->GetNoiseTexture());

		const glm::vec2 viewportScale = glm::vec2(resolutionScales[ssaoSettings.resolutionScale]);
		baseMaterial->WriteToBuffer(ssaoBuffer, ssaoBufferName, "viewportScale", viewportScale);
		baseMaterial->WriteToBuffer(ssaoBuffer, ssaoBufferName, "kernelSize", ssaoSettings.kernelSize);
		baseMaterial->WriteToBuffer(ssaoBuffer, ssaoBufferName, "noiseSize", ssaoSettings.noiseSize);
		baseMaterial->WriteToBuffer(ssaoBuffer, ssaoBufferName, "aoScale", ssaoSettings.aoScale);
		baseMaterial->WriteToBuffer(ssaoBuffer, ssaoBufferName, "samples", ssaoRenderer->GetSamples());
		baseMaterial->WriteToBuffer(ssaoBuffer, ssaoBufferName, "radius", ssaoSettings.radius);
		baseMaterial->WriteToBuffer(ssaoBuffer, ssaoBufferName, "bias", ssaoSettings.bias);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateSSAOBlur()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSAOBlur;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSAOBlur);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSAOBlur.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::SSAO& ssaoSettings = renderInfo.scene->GetGraphicsSettings().ssao;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[ssaoSettings.resolutionBlurScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t);
		createInfo.filepath = passName;
		createInfo.name = passName;
		createInfo.format = Format::R8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = true;

		std::shared_ptr<Texture> ssaoBlurTexture = renderInfo.renderView->GetStorageImage(passName);
		if (!ssaoSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage(passName);

			return;
		}
		else
		{
			if (!renderInfo.renderView->GetStorageImage(passName))
			{
				ssaoBlurTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage(passName, ssaoBlurTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName)->WriteTextureToAllFrames("outColor", ssaoBlurTexture);
			}
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		if (currentViewportSize != ssaoBlurTexture->GetSize())
		{
			const std::shared_ptr<Texture> ssaoBlurTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage(passName, ssaoBlurTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", ssaoBlurTexture);
		}

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateSSS()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSS;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSS);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSS.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::Shadows::SSS& sssSettings = renderInfo.scene->GetGraphicsSettings().shadows.sss;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[sssSettings.resolutionScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t);
		createInfo.filepath = passName;
		createInfo.name = passName;
		createInfo.format = Format::R8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = true;

		std::shared_ptr<Texture> sssTexture = renderInfo.renderView->GetStorageImage(passName);

		if (!sssSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage(passName);

			return;
		}
		else
		{
			if (!renderInfo.renderView->GetStorageImage(passName))
			{
				sssTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage(passName, sssTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView,
					pipeline,
					Pipeline::DescriptorSetIndexType::RENDERER,
					passName)->WriteTextureToAllFrames("outColor", sssTexture);
			}
		}

		if (currentViewportSize != sssTexture->GetSize())
		{
			const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
				renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
			const std::shared_ptr<Texture> sssTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage(passName, sssTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", sssTexture);
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

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

		const glm::vec2 viewportScale = glm::vec2(resolutionScales[sssSettings.resolutionScale]);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.maxSteps", sssSettings.maxSteps);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.maxRayDistance", sssSettings.maxRayDistance);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.maxDistance", sssSettings.maxDistance);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.minThickness", sssSettings.minThickness);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.maxThickness", sssSettings.maxThickness);
		deferredBaseMaterial->WriteToBuffer(lightsBuffer, lightsBufferName, "sss.viewportScale", viewportScale);

		renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateSSSBlur()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = SSSBlur;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(SSSBlur);

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "SSSBlur.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const GraphicsSettings::Shadows::SSS& sssSettings = renderInfo.scene->GetGraphicsSettings().shadows.sss;
		constexpr float resolutionScales[] = { 0.25f, 0.5f, 0.75f, 1.0f };
		const glm::ivec2 currentViewportSize = glm::vec2(renderInfo.viewportSize) * glm::vec2(resolutionScales[sssSettings.resolutionBlurScale]);

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t);
		createInfo.filepath = passName;
		createInfo.name = passName;
		createInfo.format = Format::R8_UNORM;
		createInfo.size = currentViewportSize;
		createInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED };
		createInfo.isMultiBuffered = true;

		std::shared_ptr<Texture> sssBlurTexture = renderInfo.renderView->GetStorageImage(passName);
		if (!sssSettings.isEnabled)
		{
			renderInfo.renderView->DeleteUniformWriter(passName);
			renderInfo.renderView->DeleteStorageImage(passName);

			return;
		}
		else
		{
			if (!renderInfo.renderView->GetStorageImage(passName))
			{
				sssBlurTexture = Texture::Create(createInfo);
				renderInfo.renderView->SetStorageImage(passName, sssBlurTexture);
				GetOrCreateUniformWriter(
					renderInfo.renderView,
					pipeline,
					Pipeline::DescriptorSetIndexType::RENDERER,
					passName)->WriteTextureToAllFrames("outColor", sssBlurTexture);
			}
		}

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);
		if (currentViewportSize != sssBlurTexture->GetSize())
		{
			const std::shared_ptr<Texture> ssaoBlurTexture = Texture::Create(createInfo);
			renderInfo.renderView->SetStorageImage(passName, ssaoBlurTexture);
			renderUniformWriter->WriteTextureToAllFrames("outColor", ssaoBlurTexture);
		}

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, { 1.0f, 1.0f, 0.0f }, renderInfo.frame);

			glm::uvec2 groupCount = currentViewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);

		}

		renderInfo.renderer->EndCommandLabel(renderInfo.frame);
	};

	CreateComputePass(createInfo);
}

void RenderPassManager::CreateHiZPyramid()
{
	ComputePass::CreateInfo createInfo{};
	createInfo.type = Pass::Type::COMPUTE;
	createInfo.name = HiZPyramid;

	createInfo.executeCallback = [this, passName = createInfo.name](const RenderPass::RenderCallbackInfo& renderInfo)
	{
		PROFILER_SCOPE(HiZPyramid);

		if (!renderInfo.scene->GetGraphicsSettings().hiZOcclusionCulling.isEnabled)
		{
			renderInfo.renderView->DeleteBuffer("HiZBuffer");
			renderInfo.renderView->DeleteBuffer("HiZAtomicBuffer");
			renderInfo.renderView->DeleteStorageImage(passName);
			renderInfo.renderView->DeleteUniformWriter(passName);

			return;
		}

		const std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(
			std::filesystem::path("Materials") / "HiZPyramid.basemat");
		const std::shared_ptr<Pipeline> pipeline = baseMaterial->GetPipeline(passName);
		if (!pipeline)
		{
			return;
		}

		const glm::ivec2 viewportSize = renderInfo.viewportSize;
		const uint32_t mipLevelCount = static_cast<uint32_t>(std::floor(std::log2(std::max(
			viewportSize.x, viewportSize.y)))) + 1;

		Texture::CreateInfo textureCreateInfo{};
		textureCreateInfo.aspectMask = Texture::AspectMask::COLOR;
		textureCreateInfo.instanceSize = sizeof(float);
		textureCreateInfo.filepath = passName;
		textureCreateInfo.name = passName;
		textureCreateInfo.format = Format::R32_SFLOAT;
		textureCreateInfo.size = viewportSize;
		textureCreateInfo.usage = { Texture::Usage::STORAGE, Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_SRC, Texture::Usage::TRANSFER_DST };
		textureCreateInfo.isMultiBuffered = true;
		textureCreateInfo.mipLevels = mipLevelCount;

		const std::shared_ptr<UniformWriter> renderUniformWriter = GetOrCreateUniformWriter(
			renderInfo.renderView, pipeline, Pipeline::DescriptorSetIndexType::RENDERER, passName);

		bool needToWriteToUniformWriter = false;
		std::shared_ptr<Texture> hiZTexture = renderInfo.renderView->GetStorageImage(passName);
		if (!hiZTexture)
		{
			hiZTexture = Texture::Create(textureCreateInfo);
			renderInfo.renderView->SetStorageImage(passName, hiZTexture);
			needToWriteToUniformWriter = true;
		}

		if (viewportSize != hiZTexture->GetSize())
		{
			hiZTexture = Texture::Create(textureCreateInfo);
			renderInfo.renderView->SetStorageImage(passName, hiZTexture);
		}

		if (needToWriteToUniformWriter)
		{
			std::vector<UniformWriter::TextureInfo> textureInfos(mipLevelCount);
			for (size_t i = 0; i < mipLevelCount; i++)
			{
				UniformWriter::TextureInfo& textureInfo = textureInfos[i];
				textureInfo.texture = hiZTexture;
				textureInfo.baseMipLevel = i;
			}
			
			renderUniformWriter->WriteTexturesToAllFrames("outHiZPyramid", textureInfos, 0);
		}

		const std::shared_ptr<Buffer> hiZBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			"HiZBuffer",
			{},
			{ Buffer::Usage::UNIFORM_BUFFER },
			MemoryType::CPU,
			true);

		const std::shared_ptr<Buffer> hiZAtomicBuffer = GetOrCreateBuffer(
			renderInfo.renderView,
			renderUniformWriter,
			"HiZAtomicBuffer",
			{},
			{ Buffer::Usage::STORAGE_BUFFER },
			MemoryType::GPU,
			true);

		WriteRenderViews(renderInfo.renderView, renderInfo.scene->GetRenderView(), pipeline, renderUniformWriter);

		uint32_t workgroupsX = (renderInfo.viewportSize.x + 64 - 1) / 64;
    	uint32_t workgroupsY = (renderInfo.viewportSize.y + 64 - 1) / 64;
		uint32_t workgroupCount = workgroupsX * workgroupsY;
		
		baseMaterial->WriteToBuffer(hiZBuffer, "HiZBuffer", "sourceSize", renderInfo.viewportSize);
		baseMaterial->WriteToBuffer(hiZBuffer, "HiZBuffer", "mipLevelCount", mipLevelCount);
		baseMaterial->WriteToBuffer(hiZBuffer, "HiZBuffer", "workgroupCount", workgroupCount);

		std::vector<NativeHandle> uniformWriterNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		GetUniformWriters(pipeline, baseMaterial, nullptr, renderInfo, uniformWriters, uniformWriterNativeHandles);
		if (FlushUniformWriters(uniformWriters))
		{
			renderInfo.renderer->BeginCommandLabel(passName, topLevelRenderPassDebugColor, renderInfo.frame);
			
			renderInfo.renderer->FillBuffer(hiZAtomicBuffer->GetNativeHandle(), hiZAtomicBuffer->GetSize(), 0, 0, renderInfo.frame);

			glm::uvec2 groupCount = viewportSize / glm::ivec2(16, 16);
			groupCount += glm::uvec2(1, 1);
			renderInfo.renderer->Compute(pipeline, { groupCount.x, groupCount.y, 1 }, uniformWriterNativeHandles, renderInfo.frame);

			renderInfo.renderer->EndCommandLabel(renderInfo.frame);
		}
	};

	CreateComputePass(createInfo);
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

		const int isSSREnabled = graphicsSettings.ssr.isEnabled;
		const int SSRMipLevels = isSSREnabled ? renderInfo.renderView->GetStorageImage(SSRBlur)->GetMipLevels() * graphicsSettings.ssr.mipMultiplier : 0;
		baseMaterial->WriteToBuffer(toneMappingBufferBuffer, toneMappingBufferBufferName, "toneMapperIndex", graphicsSettings.postProcess.toneMapper);
		baseMaterial->WriteToBuffer(toneMappingBufferBuffer, toneMappingBufferBufferName, "gamma", graphicsSettings.postProcess.gamma);
		baseMaterial->WriteToBuffer(toneMappingBufferBuffer, toneMappingBufferBufferName, "isSSREnabled", isSSREnabled);
		baseMaterial->WriteToBuffer(toneMappingBufferBuffer, toneMappingBufferBufferName, "SSRMipLevels", SSRMipLevels);

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
