#pragma once

#include "Core.h"
#include "LineRenderer.h"
#include "SSAORenderer.h"
#include "CSMRenderer.h"

#include "../Graphics/ComputePass.h"
#include "../Graphics/RenderPass.h"
#include "../Graphics/Pipeline.h"
#include "../Graphics/Mesh.h"

namespace Pengine
{

	class PENGINE_API RenderPassManager
	{
	public:
		static RenderPassManager& GetInstance();

		RenderPassManager(const RenderPassManager&) = delete;
		RenderPassManager& operator=(const RenderPassManager&) = delete;

		void Initialize();

		std::shared_ptr<RenderPass> CreateRenderPass(const RenderPass::CreateInfo& createInfo);

		std::shared_ptr<ComputePass> CreateComputePass(const ComputePass::CreateInfo& createInfo);

		std::shared_ptr<Pass> GetPass(const std::string& name) const;

		std::shared_ptr<RenderPass> GetRenderPass(const std::string& name) const;

		std::shared_ptr<ComputePass> GetComputePass(const std::string& name) const;

		size_t GetPassCount() const { return m_PassesByName.size(); }

		void ShutDown();

		static void GetUniformWriters(
			std::shared_ptr<class Pipeline> pipeline,
			std::shared_ptr<class BaseMaterial> baseMaterial,
			std::shared_ptr<class Material> material,
			const RenderPass::RenderCallbackInfo& renderInfo,
			std::vector<std::shared_ptr<UniformWriter>>& uniformWriters,
			std::vector<NativeHandle>& uniformWriterNativeHandles);

		static bool BindAndFlushUniformWriters(
			std::shared_ptr<class Pipeline> pipeline,
			std::shared_ptr<class BaseMaterial> baseMaterial,
			std::shared_ptr<class Material> material,
			const RenderPass::RenderCallbackInfo& renderInfo,
			const std::vector<Pipeline::DescriptorSetIndexType>& descriptorSetIndexTypes,
			std::shared_ptr<UniformWriter> objectUniformWriter = nullptr);

		static void PrepareUniformsPerViewportBeforeDraw(const RenderPass::RenderCallbackInfo& renderInfo);

		static void ProcessEntities(const RenderPass::RenderCallbackInfo& renderInfo);

		static void ProcessLights(const RenderPass::RenderCallbackInfo& renderInfo);

		std::shared_ptr<Texture> ScaleTexture(
			std::shared_ptr<Texture> sourceTexture,
			const glm::ivec2& dstSize);

	private:
		struct EntitiesByMesh
		{
			std::unordered_map<std::shared_ptr<class Mesh>, std::vector<std::vector<entt::entity>>> instanced;

			struct Single
			{
				std::shared_ptr<class Mesh> mesh;
				entt::entity entity;
				uint32_t lod;
			};

			std::vector<Single> single;
		};

		using MeshesByMaterial = std::unordered_map<std::shared_ptr<class Material>, EntitiesByMesh>;
		using RenderableEntities = std::unordered_map<std::shared_ptr<class BaseMaterial>, MeshesByMaterial>;

		struct MultiPassLightData : public CustomData
		{
			struct PointLight
			{
				std::array<glm::mat4, 6> viewProjectionMat4;
				entt::entity entity;
				uint32_t index;
			};

			struct SpotLight
			{
				glm::mat4 viewProjectionMat4;
				entt::entity entity;
				uint32_t index;
			};

			std::vector<PointLight> pointLights;
			std::vector<SpotLight> spotLights;
		};

		struct MultiPassEntityData : public CustomData
		{
			std::shared_ptr<Buffer> entityBuffer;
			std::shared_ptr<UniformWriter> entityUniformWriter;
			
			uint32_t totalEntityCount = 0;
		
			struct PassData
			{
				struct PipelineInfo
				{
					uint32_t maxDrawCount = 0;
					int id = -1;
				};
				
				std::unordered_map<std::shared_ptr<Pipeline>, PipelineInfo> pipelineInfos;
				std::map<int, std::shared_ptr<Pipeline>> sortedPipelines;
				uint32_t entityCount = 0;
			};
			
			std::unordered_map<std::string, PassData> passesByName;
		};

		struct InstanceData
		{
			size_t materialBuffer;
			glm::mat4 transform;
			glm::mat3 inverseTransform;
		};

		RenderPassManager() = default;
		~RenderPassManager() = default;

		static void GetVertexBuffers(
			std::shared_ptr<class Pipeline> pipeline,
			std::shared_ptr<class Mesh> mesh,
			std::vector<NativeHandle>& vertexBuffers,
			std::vector<size_t>& vertexBufferOffsets);

		void CreateComputeIndirectDrawGBuffer();

		void CreateComputeIndirectDrawCSM();

		void CreateGBuffer();

		void CreateDeferred();

		void CreateDefaultReflection();

		void CreateAtmosphere();

		void CreateTransparent();

		void CreateCSM();

		void CreatePointLightShadows();

		void CreateSpotLightShadows();

		void CreateBloom();

		void CreateSSR();

		void CreateSSRBlur();

		void CreateSSAO();

		void CreateSSAOBlur();

		void CreateSSS();

		void CreateSSSBlur();

		void CreateHiZPyramid();

		void CreateUI();

		void CreateDecalPass();

		void CreateToneMappingPass();

		void CreateAntiAliasingAndComposePass();

		static std::shared_ptr<class UniformWriter> ResolveUniformWriter(
			Pipeline::DescriptorSetIndexType type,
			const std::string& name,
			const std::shared_ptr<class BaseMaterial>& baseMaterial,
			const std::shared_ptr<class Material>& material,
			const RenderPass::RenderCallbackInfo& renderInfo,
			const std::shared_ptr<class UniformWriter>& objectUniformWriter);

		static bool FlushUniformWriters(const std::vector<std::shared_ptr<class UniformWriter>>& uniformWriters);

		static void WriteRenderViews(
			std::shared_ptr<class RenderView> cameraRenderView,
			std::shared_ptr<class RenderView> sceneRenderView,
			std::shared_ptr<class Pipeline> pipeline,
			std::shared_ptr<class UniformWriter> uniformWriter);

		static std::shared_ptr<class UniformWriter> GetOrCreateUniformWriter(
			std::shared_ptr<class RenderView> renderView,
			std::shared_ptr<class Pipeline> pipeline,
			Pipeline::DescriptorSetIndexType descriptorSetIndexType,
			const std::string& uniformWriterName,
			const std::string& uniformWriterIndexByName = {},
			const bool isMultiBuffered = true);

		static std::shared_ptr<class Buffer> GetOrCreateBuffer(
			std::shared_ptr<class RenderView> renderView,
			std::shared_ptr<class UniformWriter> uniformWriter,
			const std::string& bufferName,
			const std::string& setBufferName = {},
			const std::vector<Buffer::Usage>& usages = { Buffer::Usage::UNIFORM_BUFFER },
			const MemoryType memoryType = MemoryType::CPU,
			const bool isMultiBuffered = false);

		static size_t GetLod(
			const glm::vec3& cameraPosition,
			const glm::vec3& meshPosition,
			const float radius,
			const std::vector<Mesh::Lod>& distanceThresholds);

		std::unordered_map<std::string, std::shared_ptr<Pass>> m_PassesByName;
	};

}
