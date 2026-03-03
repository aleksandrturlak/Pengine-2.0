#pragma once

#include "../Core/Core.h"
#include "../Graphics/GraphicsPipeline.h"

#include <vulkan/vulkan.h>
#include <SPIRV-Reflect/spirv_reflect.h>

#include <shaderc/shaderc.hpp>

namespace Pengine::Vk
{

	struct PipelineConfigInfo
	{
		VkPipelineViewportStateCreateInfo viewportInfo;
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
		VkPipelineRasterizationStateCreateInfo rasterizationInfo;
		VkPipelineMultisampleStateCreateInfo multisampleInfo;
		std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
		VkPipelineColorBlendStateCreateInfo colorBlendInfo;
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
		std::vector<VkDynamicState> dynamicStateEnables;
		VkPipelineDynamicStateCreateInfo dynamicStateInfo;
		VkPipelineLayout pipelineLayout = nullptr;
		VkRenderPass renderPass = nullptr;
		uint32_t subpass = 0;
	};

	class PENGINE_API VulkanGraphicsPipeline final : public GraphicsPipeline
	{
	public:
		static void DefaultPipelineConfigInfo(PipelineConfigInfo& pipelineConfigInfo);

		explicit VulkanGraphicsPipeline(const CreateGraphicsInfo& createGraphicsInfo);
		virtual ~VulkanGraphicsPipeline() override;
		VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
		VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;

		static VkCullModeFlagBits ConvertCullMode(CullMode cullMode);

		static CullMode ConvertCullMode(VkCullModeFlagBits cullMode);

		static VkPrimitiveTopology ConvertTopologyMode(TopologyMode topologyMode);

		static TopologyMode ConvertTopologyMode(VkPrimitiveTopology topologyMode);

		static VkPolygonMode ConvertPolygonMode(PolygonMode polygonMode);

		static PolygonMode ConvertPolygonMode(VkPolygonMode polygonMode);

		static VkPipelineColorBlendAttachmentState ConvertBlendAttachmentState(const BlendStateAttachment& blendStateAttachment);

		static BlendStateAttachment ConvertBlendAttachmentState(const VkPipelineColorBlendAttachmentState& blendStateAttachment);

		static VkBlendFactor ConvertBlendFactor(BlendStateAttachment::BlendFactor blendFactor);

		static BlendStateAttachment::BlendFactor ConvertBlendFactor(VkBlendFactor blendFactor);

		static VkBlendOp ConvertBlendOp(BlendStateAttachment::BlendOp blendOp);

		static BlendStateAttachment::BlendOp ConvertBlendOp(VkBlendOp blendOp);

		VkPipeline GetPipeline() const { return m_GraphicsPipeline; }

		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

	private:
		static std::vector<VkVertexInputBindingDescription> CreateBindingDescriptions(
			const ShaderReflection::ReflectShaderModule& reflectShaderModule,
			const std::vector<BindingDescription>& bindingDescriptions);

		static std::vector<VkVertexInputAttributeDescription> CreateAttributeDescriptions(
			const ShaderReflection::ReflectShaderModule& reflectShaderModule,
			const std::vector<BindingDescription>& bindingDescriptions);

		static VkVertexInputRate ConvertVertexInputRate(InputRate vertexInputRate);

		static InputRate ConvertVertexInputRate(VkVertexInputRate vertexInputRate);

		static void CollectBindingsByDescriptorSet(
			std::map<uint32_t, std::vector<ShaderReflection::ReflectDescriptorSetBinding>>& bindingsByDescriptorSet,
			const std::vector<ShaderReflection::ReflectDescriptorSetLayout>& setLayouts,
			const std::filesystem::path& debugShaderFilepath);

		VkPipeline m_GraphicsPipeline{};
		VkPipelineLayout m_PipelineLayout{};
	};

}
