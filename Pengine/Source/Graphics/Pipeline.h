#pragma once

#include "../Core/Core.h"

#include "RenderPass.h"
#include "UniformLayout.h"
#include "ShaderModule.h"

namespace Pengine
{

	class PENGINE_API Pipeline
	{
	public:
		enum class Type
		{
			GRAPHICS,
			COMPUTE
		};

		enum class DescriptorSetIndexType
		{
			BINDLESS,
			RENDERER,
			SCENE,
			RENDERPASS,
			BASE_MATERIAL,
			MATERIAL,
			OBJECT,
			BUFFER_DEVICE_ADDRESS,
			TOTAL
		};

		struct UniformBufferInfo
		{
			std::unordered_map<std::string, int> intValuesByName;
			std::unordered_map<std::string, float> floatValuesByName;
			std::unordered_map<std::string, glm::vec2> vec2ValuesByName;
			std::unordered_map<std::string, glm::vec3> vec3ValuesByName;
			std::unordered_map<std::string, glm::vec4> vec4ValuesByName;
			std::unordered_map<std::string, std::string> texturesByName;
		};

		/**
		 * Used to attach framebuffer attachments and storage images to the descriptor set of RENDERER type.
		 * Textures will be taken from render targets of the scene and the camera.
		 * If no attachment index then the texture will be considered as storage image.
		 */
		struct TextureAttachmentInfo
		{
			std::string name;
			std::string defaultName;
			uint32_t attachmentIndex = 0;
		};

		struct UniformInfo
		{
			// For descriptor sets of BASEMATERIAL, MATERIAL type.
			std::unordered_map<std::string, UniformBufferInfo> uniformBuffersByName;
			std::unordered_map<std::string, std::string> texturesByName;

			// For descriptor sets of RENDERER type.
			std::unordered_map<std::string, TextureAttachmentInfo> textureAttachmentsByName;
		};

		struct CreateComputeInfo
		{
			std::map<DescriptorSetIndexType, std::map<std::string, uint32_t>> descriptorSetIndicesByType;
			UniformInfo uniformInfo;
			std::map<ShaderModule::Type, std::filesystem::path> shaderFilepathsByType;
		};

		static Type GetPipelineType(const std::map<ShaderModule::Type, std::filesystem::path>& shaderFilepathsByType);

		explicit Pipeline(Type type);
		virtual ~Pipeline() = default;
		Pipeline(const Pipeline&) = delete;
		Pipeline(Pipeline&&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;
		Pipeline& operator=(Pipeline&&) = delete;

		std::shared_ptr<UniformLayout> GetUniformLayout(const uint32_t descriptorSet) const;

		const std::map<uint32_t, std::shared_ptr<UniformLayout>>& GetUniformLayouts() const { return m_UniformLayoutsByDescriptorSet; }

		virtual const UniformInfo& GetUniformInfo() const = 0;

		virtual std::map<std::string, uint32_t> GetDescriptorSetIndexByType(const DescriptorSetIndexType type) const = 0;

		const std::optional<uint32_t> GetDescriptorSetIndexByType(const DescriptorSetIndexType type, const std::string& name) const;

		const std::map<uint32_t, std::pair<DescriptorSetIndexType, std::string>>& GetSortedDescriptorSets() const { return m_SortedDescriptorSets;  }

		Type GetType() const { return m_Type; }

	protected:

		Type m_Type;

		std::map<ShaderModule::Type, std::shared_ptr<ShaderModule>> m_ShaderModulesByType;
		std::map<uint32_t, std::shared_ptr<UniformLayout>> m_UniformLayoutsByDescriptorSet;
		std::map<uint32_t, std::pair<DescriptorSetIndexType, std::string>> m_SortedDescriptorSets;
	};

}
