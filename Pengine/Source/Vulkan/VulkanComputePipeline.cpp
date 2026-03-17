#include "VulkanComputePipeline.h"

#include "VulkanDevice.h"
#include "VulkanPipelineUtils.h"
#include "VulkanUniformLayout.h"
#include "VulkanShaderModule.h"

#include "../Core/Logger.h"
#include "../Graphics/ShaderModuleManager.h"

using namespace Pengine;
using namespace Vk;

VulkanComputePipeline::VulkanComputePipeline(const CreateComputeInfo& createComputeInfo)
	: ComputePipeline(createComputeInfo)
{
	const auto filepath = createComputeInfo.shaderFilepathsByType.at(ShaderModule::Type::COMPUTE);

	if (filepath.empty())
	{
		FATAL_ERROR("Failed to create compute pipeline, compute shader filepath is empty!");
	}

	const std::shared_ptr<ShaderModule> shaderModule = ShaderModuleManager::GetInstance().GetOrCreateShaderModule(filepath, ShaderModule::Type::COMPUTE);
	if (!shaderModule->IsValid())
	{
		FATAL_ERROR(std::format("Failed to get shader module {}, it is invalid!", filepath.string()));
	}

	m_ShaderModulesByType[ShaderModule::Type::COMPUTE] = shaderModule;

	const ShaderReflection::ReflectShaderModule& reflection = std::static_pointer_cast<VulkanShaderModule>(shaderModule)->GetReflection();

	std::map<uint32_t, std::vector<ShaderReflection::ReflectDescriptorSetBinding>> bindingsByDescriptorSet;
	for (const auto& [set, bindings] : reflection.setLayouts)
	{
		bindingsByDescriptorSet[set] = bindings;
	}

	m_UniformLayoutsByDescriptorSet = VulkanPipelineUtils::CreateDescriptorSetLayouts(bindingsByDescriptorSet);

	VkPipelineShaderStageCreateInfo shaderStage{};

	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = VulkanPipelineUtils::ConvertShaderStage(ShaderModule::Type::COMPUTE);
	shaderStage.module = std::static_pointer_cast<VulkanShaderModule>(shaderModule)->GetShaderModule();
	shaderStage.pName = "main";
	shaderStage.flags = 0;
	shaderStage.pNext = nullptr;
	shaderStage.pSpecializationInfo = nullptr;

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	for (const auto& [set, uniformLayout] : m_UniformLayoutsByDescriptorSet)
	{
		descriptorSetLayouts.emplace_back(std::static_pointer_cast<VulkanUniformLayout>(uniformLayout)->GetDescriptorSetLayout());
	}

	std::vector<VkPushConstantRange> pushConstantRanges;
	for (const auto& range : reflection.pushConstantRanges)
	{
		VkPushConstantRange vkRange{};
		vkRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		vkRange.offset = range.offset;
		vkRange.size = range.size;
		pushConstantRanges.push_back(vkRange);
	}

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

	if (vkCreatePipelineLayout(GetVkDevice()->GetDevice(), &pipelineLayoutCreateInfo,
		nullptr, &m_PipelineLayout) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to create pipeline layout!");
	}

	VkComputePipelineCreateInfo vkComputePipelineCreateInfo{};
	vkComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	vkComputePipelineCreateInfo.layout = m_PipelineLayout;
	vkComputePipelineCreateInfo.stage = shaderStage;

	vkComputePipelineCreateInfo.basePipelineIndex = -1;
	vkComputePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateComputePipelines(GetVkDevice()->GetDevice(), VK_NULL_HANDLE, 1,
		&vkComputePipelineCreateInfo, nullptr, &m_ComputePipeline) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to create graphics pipeline!");
	}
}

VulkanComputePipeline::~VulkanComputePipeline()
{
	for (auto& [type, shaderModule] : m_ShaderModulesByType)
	{
		ShaderModuleManager::GetInstance().DeleteShaderModule(shaderModule);
	}
	m_ShaderModulesByType.clear();

	GetVkDevice()->DeleteResource([pipelineLayout = m_PipelineLayout, computePipeline = m_ComputePipeline]()
	{
		vkDestroyPipelineLayout(GetVkDevice()->GetDevice(), pipelineLayout, nullptr);
		vkDestroyPipeline(GetVkDevice()->GetDevice(), computePipeline, nullptr);
	});
}
