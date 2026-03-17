#include "VulkanGraphicsPipeline.h"

#include "VulkanDevice.h"
#include "VulkanRenderPass.h"
#include "VulkanUniformLayout.h"
#include "VulkanShaderModule.h"
#include "VulkanFormat.h"
#include "VulkanPipelineUtils.h"

#include "../Core/Logger.h"
#include "../Core/Profiler.h"
#include "../Graphics/ShaderModuleManager.h"

using namespace Pengine;
using namespace Vk;

VulkanGraphicsPipeline::VulkanGraphicsPipeline(const CreateGraphicsInfo& createGraphicsInfo)
	: GraphicsPipeline(createGraphicsInfo)
{
	std::map<ShaderModule::Type, VkShaderModule> shaderModulesByType;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	shaderStages.reserve(createGraphicsInfo.shaderFilepathsByType.size());

	std::map<uint32_t, std::vector<ShaderReflection::ReflectDescriptorSetBinding>> bindingsByDescriptorSet;
	for (const auto& [type, filepath] : createGraphicsInfo.shaderFilepathsByType)
	{
		const std::shared_ptr<ShaderModule> shaderModule = ShaderModuleManager::GetInstance().GetOrCreateShaderModule(filepath, type);
		if (!shaderModule->IsValid())
		{
			FATAL_ERROR(std::format("Failed to get shader module {}, it is invalid!", filepath.string()));
		}

		m_ShaderModulesByType[type] = shaderModule;

		CollectBindingsByDescriptorSet(bindingsByDescriptorSet, shaderModule->GetReflection().setLayouts, filepath);

		VkPipelineShaderStageCreateInfo& shaderStage = shaderStages.emplace_back();
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = VulkanPipelineUtils::ConvertShaderStage(type);
		shaderStage.module = std::static_pointer_cast<VulkanShaderModule>(shaderModule)->GetShaderModule();
		shaderStage.pName = "main";
		shaderStage.flags = 0;
		shaderStage.pNext = nullptr;
		shaderStage.pSpecializationInfo = nullptr;
	}

	m_UniformLayoutsByDescriptorSet = VulkanPipelineUtils::CreateDescriptorSetLayouts(bindingsByDescriptorSet);

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	for (const auto& [set, uniformLayout] : m_UniformLayoutsByDescriptorSet)
	{
		descriptorSetLayouts.emplace_back(std::static_pointer_cast<VulkanUniformLayout>(uniformLayout)->GetDescriptorSetLayout());
	}

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayouts.size();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(GetVkDevice()->GetDevice(), &pipelineLayoutCreateInfo,
		nullptr, &m_PipelineLayout) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to create pipeline layout!");
	}

	PipelineConfigInfo pipelineConfigInfo{};
	DefaultPipelineConfigInfo(pipelineConfigInfo);

	pipelineConfigInfo.renderPass = std::static_pointer_cast<VulkanRenderPass>(
		createGraphicsInfo.renderPass)->GetRenderPass();
	pipelineConfigInfo.pipelineLayout = m_PipelineLayout;
	pipelineConfigInfo.depthStencilInfo.depthWriteEnable = static_cast<VkBool32>(createGraphicsInfo.depthWrite);
	pipelineConfigInfo.depthStencilInfo.depthTestEnable = static_cast<VkBool32>(createGraphicsInfo.depthTest);
	pipelineConfigInfo.depthStencilInfo.depthCompareOp = static_cast<VkCompareOp>(createGraphicsInfo.depthCompare);
	pipelineConfigInfo.depthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
	pipelineConfigInfo.rasterizationInfo.depthClampEnable = createGraphicsInfo.depthClamp;
	pipelineConfigInfo.rasterizationInfo.cullMode = ConvertCullMode(createGraphicsInfo.cullMode);
	pipelineConfigInfo.inputAssemblyInfo.topology = ConvertTopologyMode(createGraphicsInfo.topologyMode);
	pipelineConfigInfo.rasterizationInfo.polygonMode = ConvertPolygonMode(createGraphicsInfo.polygonMode);
	pipelineConfigInfo.colorBlendAttachments.clear();
	for (const auto& blendStateAttachment : createGraphicsInfo.colorBlendStateAttachments)
	{
		VkPipelineColorBlendAttachmentState colorBlendState = ConvertBlendAttachmentState(blendStateAttachment);
		colorBlendState.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		pipelineConfigInfo.colorBlendAttachments.emplace_back(colorBlendState);
	}
	pipelineConfigInfo.colorBlendInfo.attachmentCount = pipelineConfigInfo.colorBlendAttachments.size();
	pipelineConfigInfo.colorBlendInfo.pAttachments = pipelineConfigInfo.colorBlendAttachments.data();

	const auto& vertexReflection = m_ShaderModulesByType[ShaderModule::Type::VERTEX]->GetReflection();
	auto bindingDescriptions = CreateBindingDescriptions(vertexReflection, createGraphicsInfo.bindingDescriptions);
	auto attributeDescriptions = CreateAttributeDescriptions(vertexReflection, createGraphicsInfo.bindingDescriptions);
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	vertexInputCreateInfo.pVertexBindingDescriptions = bindingDescriptions.data();

	VkGraphicsPipelineCreateInfo vkPipelineCreateInfo{};
	vkPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	vkPipelineCreateInfo.stageCount = shaderStages.size();
	vkPipelineCreateInfo.pStages = shaderStages.data();
	vkPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	vkPipelineCreateInfo.pInputAssemblyState = &pipelineConfigInfo.inputAssemblyInfo;
	vkPipelineCreateInfo.pViewportState = &pipelineConfigInfo.viewportInfo;
	vkPipelineCreateInfo.pRasterizationState = &pipelineConfigInfo.rasterizationInfo;
	vkPipelineCreateInfo.pMultisampleState = &pipelineConfigInfo.multisampleInfo;
	vkPipelineCreateInfo.pColorBlendState = &pipelineConfigInfo.colorBlendInfo;
	vkPipelineCreateInfo.pDepthStencilState = &pipelineConfigInfo.depthStencilInfo;
	vkPipelineCreateInfo.pDynamicState = &pipelineConfigInfo.dynamicStateInfo;

	vkPipelineCreateInfo.layout = pipelineConfigInfo.pipelineLayout;
	vkPipelineCreateInfo.renderPass = pipelineConfigInfo.renderPass;
	vkPipelineCreateInfo.subpass = pipelineConfigInfo.subpass;

	vkPipelineCreateInfo.basePipelineIndex = -1;
	vkPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(GetVkDevice()->GetDevice(), VK_NULL_HANDLE, 1,
		&vkPipelineCreateInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to create graphics pipeline!");
	}
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
{
	for (auto& [type, shaderModule] : m_ShaderModulesByType)
	{
		ShaderModuleManager::GetInstance().DeleteShaderModule(shaderModule);
	}
	m_ShaderModulesByType.clear();

	GetVkDevice()->DeleteResource([pipelineLayout = m_PipelineLayout, graphicsPipeline = m_GraphicsPipeline]()
	{
		vkDestroyPipelineLayout(GetVkDevice()->GetDevice(), pipelineLayout, nullptr);
		vkDestroyPipeline(GetVkDevice()->GetDevice(), graphicsPipeline, nullptr);
		
	});
}

VkCullModeFlagBits VulkanGraphicsPipeline::ConvertCullMode(const CullMode cullMode)
{
	switch (cullMode)
	{
	case Pengine::GraphicsPipeline::CullMode::NONE:
		return VK_CULL_MODE_NONE;
	case Pengine::GraphicsPipeline::CullMode::FRONT:
		return VK_CULL_MODE_FRONT_BIT;
	case Pengine::GraphicsPipeline::CullMode::BACK:
		return VK_CULL_MODE_BACK_BIT;
	case Pengine::GraphicsPipeline::CullMode::FRONT_AND_BACK:
		return VK_CULL_MODE_FRONT_AND_BACK;
	}

	FATAL_ERROR("Failed to convert cull mode!");
	return VkCullModeFlagBits::VK_CULL_MODE_NONE;
}

GraphicsPipeline::CullMode VulkanGraphicsPipeline::ConvertCullMode(const VkCullModeFlagBits cullMode)
{
	switch (cullMode)
	{
	case VK_CULL_MODE_NONE:
		return Pengine::GraphicsPipeline::CullMode::NONE;
	case VK_CULL_MODE_FRONT_BIT:
		return Pengine::GraphicsPipeline::CullMode::FRONT;
	case VK_CULL_MODE_BACK_BIT:
		return Pengine::GraphicsPipeline::CullMode::BACK;
	case VK_CULL_MODE_FRONT_AND_BACK:
		return Pengine::GraphicsPipeline::CullMode::FRONT_AND_BACK;
	}

	FATAL_ERROR("Failed to convert cull mode!");
	return CullMode::NONE;
}

VkPrimitiveTopology VulkanGraphicsPipeline::ConvertTopologyMode(TopologyMode topologyMode)
{
	switch (topologyMode)
	{
	case Pengine::GraphicsPipeline::TopologyMode::POINT_LIST:
		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case Pengine::GraphicsPipeline::TopologyMode::LINE_LIST:
		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case Pengine::GraphicsPipeline::TopologyMode::TRIANGLE_LIST:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}

	FATAL_ERROR("Failed to convert polygon mode!");
	return {};
}

GraphicsPipeline::TopologyMode VulkanGraphicsPipeline::ConvertTopologyMode(VkPrimitiveTopology topologyMode)
{
	switch (topologyMode)
	{
	case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		return Pengine::GraphicsPipeline::TopologyMode::POINT_LIST;
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		return Pengine::GraphicsPipeline::TopologyMode::LINE_LIST;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		return Pengine::GraphicsPipeline::TopologyMode::TRIANGLE_LIST;
	}

	FATAL_ERROR("Failed to convert polygon mode!");
	return {};
}

VkPolygonMode VulkanGraphicsPipeline::ConvertPolygonMode(const PolygonMode polygonMode)
{
	switch (polygonMode)
	{
	case Pengine::GraphicsPipeline::PolygonMode::FILL:
		return VK_POLYGON_MODE_FILL;
	case Pengine::GraphicsPipeline::PolygonMode::LINE:
		return VK_POLYGON_MODE_LINE;
	}

	FATAL_ERROR("Failed to convert polygon mode!");
	return {};
}

GraphicsPipeline::PolygonMode VulkanGraphicsPipeline::ConvertPolygonMode(const VkPolygonMode polygonMode)
{
	switch (polygonMode)
	{
	case VK_POLYGON_MODE_FILL:
		return Pengine::GraphicsPipeline::PolygonMode::FILL;
	case VK_POLYGON_MODE_LINE:
		return Pengine::GraphicsPipeline::PolygonMode::LINE;
	}

	FATAL_ERROR("Failed to convert polygon mode!");
	return {};
}

VkPipelineColorBlendAttachmentState VulkanGraphicsPipeline::ConvertBlendAttachmentState(const BlendStateAttachment& blendStateAttachment)
{
	VkPipelineColorBlendAttachmentState vkPipelineColorBlendAttachmentState{};
	vkPipelineColorBlendAttachmentState.blendEnable = (VkBool32)blendStateAttachment.blendEnabled;

	vkPipelineColorBlendAttachmentState.colorBlendOp = ConvertBlendOp(blendStateAttachment.colorBlendOp);
	vkPipelineColorBlendAttachmentState.srcColorBlendFactor = ConvertBlendFactor(blendStateAttachment.srcColorBlendFactor);
	vkPipelineColorBlendAttachmentState.dstColorBlendFactor = ConvertBlendFactor(blendStateAttachment.dstColorBlendFactor);

	vkPipelineColorBlendAttachmentState.alphaBlendOp = ConvertBlendOp(blendStateAttachment.alphaBlendOp);
	vkPipelineColorBlendAttachmentState.srcAlphaBlendFactor = ConvertBlendFactor(blendStateAttachment.srcAlphaBlendFactor);
	vkPipelineColorBlendAttachmentState.dstAlphaBlendFactor = ConvertBlendFactor(blendStateAttachment.dstAlphaBlendFactor);

	return vkPipelineColorBlendAttachmentState;
}

GraphicsPipeline::BlendStateAttachment VulkanGraphicsPipeline::ConvertBlendAttachmentState(const VkPipelineColorBlendAttachmentState& blendStateAttachment)
{
	BlendStateAttachment pipelineColorBlendAttachmentState{};
	pipelineColorBlendAttachmentState.blendEnabled = blendStateAttachment.blendEnable;

	pipelineColorBlendAttachmentState.colorBlendOp = ConvertBlendOp(blendStateAttachment.colorBlendOp);
	pipelineColorBlendAttachmentState.srcColorBlendFactor = ConvertBlendFactor(blendStateAttachment.srcColorBlendFactor);
	pipelineColorBlendAttachmentState.dstColorBlendFactor = ConvertBlendFactor(blendStateAttachment.dstColorBlendFactor);

	pipelineColorBlendAttachmentState.alphaBlendOp = ConvertBlendOp(blendStateAttachment.alphaBlendOp);
	pipelineColorBlendAttachmentState.srcAlphaBlendFactor = ConvertBlendFactor(blendStateAttachment.srcAlphaBlendFactor);
	pipelineColorBlendAttachmentState.dstAlphaBlendFactor = ConvertBlendFactor(blendStateAttachment.dstAlphaBlendFactor);

	return pipelineColorBlendAttachmentState;
}

VkBlendFactor VulkanGraphicsPipeline::ConvertBlendFactor(BlendStateAttachment::BlendFactor blendFactor)
{
	switch (blendFactor)
	{
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ZERO:
		return VkBlendFactor::VK_BLEND_FACTOR_ZERO;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE:
		return VkBlendFactor::VK_BLEND_FACTOR_ONE;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_COLOR:
		return VkBlendFactor::VK_BLEND_FACTOR_SRC_COLOR;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_SRC_COLOR:
		return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::DST_COLOR:
		return VkBlendFactor::VK_BLEND_FACTOR_DST_COLOR;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_DST_COLOR:
		return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_ALPHA:
		return VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_SRC_ALPHA:
		return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::DST_ALPHA:
		return VkBlendFactor::VK_BLEND_FACTOR_DST_ALPHA;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_DST_ALPHA:
		return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::CONSTANT_COLOR:
		return VkBlendFactor::VK_BLEND_FACTOR_CONSTANT_COLOR;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_CONSTANT_COLOR:
		return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::CONSTANT_ALPHA:
		return VkBlendFactor::VK_BLEND_FACTOR_CONSTANT_ALPHA;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_CONSTANT_ALPHA:
		return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_ALPHA_SATURATE:
		return VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	}

	FATAL_ERROR("Failed to convert shader type!");
	return {};
}

GraphicsPipeline::BlendStateAttachment::BlendFactor VulkanGraphicsPipeline::ConvertBlendFactor(VkBlendFactor blendFactor)
{
	switch (blendFactor)
	{
	case VkBlendFactor::VK_BLEND_FACTOR_ZERO:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ZERO;
	case VkBlendFactor::VK_BLEND_FACTOR_ONE:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE;
	case VkBlendFactor::VK_BLEND_FACTOR_SRC_COLOR:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_COLOR;
	case VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_SRC_COLOR;
	case VkBlendFactor::VK_BLEND_FACTOR_DST_COLOR:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::DST_COLOR;
	case VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_DST_COLOR;
	case VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_ALPHA;
	case VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_SRC_ALPHA;
	case VkBlendFactor::VK_BLEND_FACTOR_DST_ALPHA:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::DST_ALPHA;
	case VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_DST_ALPHA;
	case VkBlendFactor::VK_BLEND_FACTOR_CONSTANT_COLOR:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::CONSTANT_COLOR;
	case VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_CONSTANT_COLOR;
	case VkBlendFactor::VK_BLEND_FACTOR_CONSTANT_ALPHA:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::CONSTANT_ALPHA;
	case VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::ONE_MINUS_CONSTANT_ALPHA;
	case VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendFactor::SRC_ALPHA_SATURATE;
	}

	FATAL_ERROR("Failed to convert shader type!");
	return {};
}

VkBlendOp VulkanGraphicsPipeline::ConvertBlendOp(BlendStateAttachment::BlendOp blendOp)
{
	switch (blendOp)
	{
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::ADD:
		return VkBlendOp::VK_BLEND_OP_ADD;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::SUBTRACT:
		return VkBlendOp::VK_BLEND_OP_SUBTRACT;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::REVERSE_SUBTRACT:
		return VkBlendOp::VK_BLEND_OP_REVERSE_SUBTRACT;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::MIN:
		return VkBlendOp::VK_BLEND_OP_MIN;
	case Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::MAX:
		return VkBlendOp::VK_BLEND_OP_MAX;
	}

	FATAL_ERROR("Failed to convert shader type!");
	return {};
}

GraphicsPipeline::BlendStateAttachment::BlendOp VulkanGraphicsPipeline::ConvertBlendOp(VkBlendOp blendOp)
{
	switch (blendOp)
	{
	case VkBlendOp::VK_BLEND_OP_ADD:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::ADD;
	case VkBlendOp::VK_BLEND_OP_SUBTRACT:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::SUBTRACT;
	case VkBlendOp::VK_BLEND_OP_REVERSE_SUBTRACT:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::REVERSE_SUBTRACT;
	case VkBlendOp::VK_BLEND_OP_MIN:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::MIN;
	case VkBlendOp::VK_BLEND_OP_MAX:
		return Pengine::GraphicsPipeline::BlendStateAttachment::BlendOp::MAX;
	}

	FATAL_ERROR("Failed to convert shader type!");
	return {};
}

std::vector<VkVertexInputBindingDescription> VulkanGraphicsPipeline::CreateBindingDescriptions(
	const ShaderReflection::ReflectShaderModule& reflectShaderModule,
	const std::vector<BindingDescription>& bindingDescriptions)
{
	std::vector<VkVertexInputBindingDescription> vkBindingDescriptions;
	vkBindingDescriptions.reserve(bindingDescriptions.size());

	for (const BindingDescription& bindingDescription : bindingDescriptions)
	{
		VkVertexInputBindingDescription& vkBindingDescription = vkBindingDescriptions.emplace_back();

		vkBindingDescription.binding = bindingDescription.binding;
		vkBindingDescription.inputRate = ConvertVertexInputRate(bindingDescription.inputRate);

		uint32_t stride = 0;
		for (const std::string& name : bindingDescription.names)
		{
			// Looking for an attribute description with a specific name.
			std::optional<ShaderReflection::AttributeDescription> foundAttributeDescription;
			for (const auto& attributeDescription : reflectShaderModule.attributeDescriptions)
			{
				if (attributeDescription.name == name)
				{
					foundAttributeDescription = attributeDescription;
					break;
				}
			}

			if (foundAttributeDescription)
			{
				stride += foundAttributeDescription->count * foundAttributeDescription->size;
			}
			else
			{
				FATAL_ERROR("Failed to find vertex input attribute with the name: " + name);
			}
		}

		vkBindingDescription.stride = stride;
	}

	return vkBindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> VulkanGraphicsPipeline::CreateAttributeDescriptions(
	const ShaderReflection::ReflectShaderModule& reflectShaderModule,
	const std::vector<BindingDescription>& bindingDescriptions)
{
	std::vector<VkVertexInputAttributeDescription> vkAttributeDescriptions;

	std::map<uint32_t, std::vector<ShaderReflection::AttributeDescription>> sortedAttributeDescriptionsByBinding;

	for (const BindingDescription& bindingDescription : bindingDescriptions)
	{
		for (const std::string& name : bindingDescription.names)
		{
			// Looking for an attribute description with a specific name.
			std::optional<ShaderReflection::AttributeDescription> foundAttributeDescription;
			for (const auto& attributeDescription : reflectShaderModule.attributeDescriptions)
			{
				if (attributeDescription.name == name)
				{
					foundAttributeDescription = attributeDescription;
					break;
				}
			}

			if (foundAttributeDescription)
			{
				sortedAttributeDescriptionsByBinding[bindingDescription.binding].emplace_back(*foundAttributeDescription);
			}
			else
			{
				FATAL_ERROR("Failed to find vertex input attribute with the name: " + name);
			}
		}
	}

	for (const auto& [binding, attributeDescriptions] : sortedAttributeDescriptionsByBinding)
	{
		uint32_t offset = 0;
		for (const auto& attributeDescription : attributeDescriptions)
		{
			for (size_t row = 0; row < attributeDescription.count; row++)
			{
				VkVertexInputAttributeDescription& vkAttributeDescription = vkAttributeDescriptions.emplace_back();
				vkAttributeDescription.binding = binding;
				vkAttributeDescription.format = ConvertFormat(attributeDescription.format);
				vkAttributeDescription.location = attributeDescription.location + row;
				vkAttributeDescription.offset = offset;

				offset += attributeDescription.size;
			}
		}
	}

	return vkAttributeDescriptions;
}

VkVertexInputRate VulkanGraphicsPipeline::ConvertVertexInputRate(const InputRate vertexInputRate)
{
	switch (vertexInputRate)
	{
	case Pengine::Vk::VulkanGraphicsPipeline::InputRate::VERTEX:
		return VK_VERTEX_INPUT_RATE_VERTEX;
	case Pengine::Vk::VulkanGraphicsPipeline::InputRate::INSTANCE:
		return VK_VERTEX_INPUT_RATE_INSTANCE;
	}

	FATAL_ERROR("Failed to convert vertex input rate!");
	return VkVertexInputRate::VK_VERTEX_INPUT_RATE_MAX_ENUM;
}

VulkanGraphicsPipeline::InputRate VulkanGraphicsPipeline::ConvertVertexInputRate(const VkVertexInputRate vertexInputRate)
{
	switch (vertexInputRate)
	{
	case VK_VERTEX_INPUT_RATE_VERTEX:
		return Pengine::Vk::VulkanGraphicsPipeline::InputRate::VERTEX;
	case VK_VERTEX_INPUT_RATE_INSTANCE:
		return Pengine::Vk::VulkanGraphicsPipeline::InputRate::INSTANCE;
	}

	FATAL_ERROR("Failed to convert vertex input rate!");
	return {};
}

void VulkanGraphicsPipeline::CollectBindingsByDescriptorSet(
	std::map<uint32_t, std::vector<ShaderReflection::ReflectDescriptorSetBinding>>& bindingsByDescriptorSet,
	const std::vector<ShaderReflection::ReflectDescriptorSetLayout>& setLayouts,
	const std::filesystem::path& debugShaderFilepath)
{
	for (const auto& [set, bindings] : setLayouts)
	{
		if (bindingsByDescriptorSet.contains(set))
		{
			for (const auto& reflectBinding : bindings)
			{
				bool foundBinding = false;
				for (const auto& finalBinding : bindingsByDescriptorSet.at(set))
				{
					if (reflectBinding.binding == finalBinding.binding)
					{
						if (reflectBinding.name == finalBinding.name &&
							reflectBinding.type == finalBinding.type)
						{
							foundBinding = true;
							break;
						}
						else
						{
							FATAL_ERROR("Shader:" + debugShaderFilepath.string() + " bindings between stages are not the same!");
						}
					}
				}

				if (!foundBinding)
				{
					bindingsByDescriptorSet.at(set).emplace_back(reflectBinding);
				}
			}
		}
		else
		{
			bindingsByDescriptorSet[set] = bindings;
		}
	}
}

void VulkanGraphicsPipeline::DefaultPipelineConfigInfo(PipelineConfigInfo& pipelineConfigInfo)
{
	pipelineConfigInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineConfigInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipelineConfigInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	pipelineConfigInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineConfigInfo.viewportInfo.viewportCount = 1;
	pipelineConfigInfo.viewportInfo.pViewports = nullptr;
	pipelineConfigInfo.viewportInfo.scissorCount = 1;
	pipelineConfigInfo.viewportInfo.pScissors = nullptr;

	pipelineConfigInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineConfigInfo.rasterizationInfo.depthClampEnable = VK_TRUE;
	pipelineConfigInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
	pipelineConfigInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineConfigInfo.rasterizationInfo.lineWidth = 1.0f;
	pipelineConfigInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
	pipelineConfigInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineConfigInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
	pipelineConfigInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;  // Optional
	pipelineConfigInfo.rasterizationInfo.depthBiasClamp = 0.0f;           // Optional
	pipelineConfigInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;     // Optional

	pipelineConfigInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineConfigInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
	pipelineConfigInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipelineConfigInfo.multisampleInfo.minSampleShading = 1.0f;           // Optional
	pipelineConfigInfo.multisampleInfo.pSampleMask = nullptr;             // Optional
	pipelineConfigInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;  // Optional
	pipelineConfigInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;       // Optional

	pipelineConfigInfo.colorBlendAttachments.resize(1);
	pipelineConfigInfo.colorBlendAttachments[0].colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	pipelineConfigInfo.colorBlendAttachments[0].blendEnable = VK_FALSE;
	pipelineConfigInfo.colorBlendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;   // Optional
	pipelineConfigInfo.colorBlendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // Optional
	pipelineConfigInfo.colorBlendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;              // Optional
	pipelineConfigInfo.colorBlendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
	pipelineConfigInfo.colorBlendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
	pipelineConfigInfo.colorBlendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

	pipelineConfigInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineConfigInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
	pipelineConfigInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;  // Optional
	pipelineConfigInfo.colorBlendInfo.attachmentCount = pipelineConfigInfo.colorBlendAttachments.size();
	pipelineConfigInfo.colorBlendInfo.pAttachments = pipelineConfigInfo.colorBlendAttachments.data();
	pipelineConfigInfo.colorBlendInfo.blendConstants[0] = 0.0f;  // Optional
	pipelineConfigInfo.colorBlendInfo.blendConstants[1] = 0.0f;  // Optional
	pipelineConfigInfo.colorBlendInfo.blendConstants[2] = 0.0f;  // Optional
	pipelineConfigInfo.colorBlendInfo.blendConstants[3] = 0.0f;  // Optional

	pipelineConfigInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineConfigInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
	pipelineConfigInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
	pipelineConfigInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	pipelineConfigInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	pipelineConfigInfo.depthStencilInfo.minDepthBounds = 0.0f;  // Optional
	pipelineConfigInfo.depthStencilInfo.maxDepthBounds = 1.0f;  // Optional
	pipelineConfigInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
	pipelineConfigInfo.depthStencilInfo.front = {};  // Optional
	pipelineConfigInfo.depthStencilInfo.back = {};   // Optional

	pipelineConfigInfo.dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	pipelineConfigInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineConfigInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(pipelineConfigInfo.dynamicStateEnables.size());
	pipelineConfigInfo.dynamicStateInfo.pDynamicStates = pipelineConfigInfo.dynamicStateEnables.data();
	pipelineConfigInfo.dynamicStateInfo.flags = 0;
}
