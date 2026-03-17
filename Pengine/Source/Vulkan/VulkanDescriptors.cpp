#include "VulkanDescriptors.h"

#include "VulkanDevice.h"

#include "../Core/Logger.h"

using namespace Pengine;
using namespace Vk;

VulkanDescriptorSetLayout::Builder & VulkanDescriptorSetLayout::Builder::AddBinding(
	const uint32_t binding,
	const VkDescriptorType descriptorType,
	const VkShaderStageFlags stageFlags,
	const uint32_t count)
{
	assert(m_Bindings.count(binding) == 0 && "Binding already in use");
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = descriptorType;
	layoutBinding.descriptorCount = count;
	layoutBinding.stageFlags = stageFlags;
	m_Bindings[binding] = layoutBinding;
	return *this;
}

std::unique_ptr<VulkanDescriptorSetLayout> VulkanDescriptorSetLayout::Builder::Build() const
{
	return std::make_unique<VulkanDescriptorSetLayout>(m_Bindings);
}

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(
	const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings)
	: m_Bindings{bindings}
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
	for (const auto& [index, binding] : bindings)
	{
		setLayoutBindings.push_back(binding);
	}

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
	descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

	if (vkCreateDescriptorSetLayout(
		GetVkDevice()->GetDevice(),
		&descriptorSetLayoutInfo,
		nullptr,
		&m_DescriptorSetLayout) != VK_SUCCESS)
	{
		FATAL_ERROR("failed to create descriptor set layout!");
	}
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
{
	GetVkDevice()->DeleteResource([descriptorSetLayout = m_DescriptorSetLayout]()
	{
		vkDestroyDescriptorSetLayout(GetVkDevice()->GetDevice(), descriptorSetLayout, nullptr);
	});
}

VulkanDescriptorPool::Builder & VulkanDescriptorPool::Builder::AddPoolSize(
	const VkDescriptorType descriptorType, const uint32_t count)
{
	m_PoolSizes.push_back({ descriptorType, count });
	return *this;
}

VulkanDescriptorPool::Builder & VulkanDescriptorPool::Builder::SetPoolFlags(
	const VkDescriptorPoolCreateFlags flags)
{
	m_PoolFlags = flags;
	return *this;
}
VulkanDescriptorPool::Builder & VulkanDescriptorPool::Builder::SetMaxSets(const uint32_t count)
{
	m_MaxSets = count;
	return *this;
}

std::shared_ptr<VulkanDescriptorPool> VulkanDescriptorPool::Builder::Build(const VkDevice device) const
{
	return std::make_unique<VulkanDescriptorPool>(m_MaxSets, m_PoolFlags, m_PoolSizes, device);
}

VulkanDescriptorPool::VulkanDescriptorPool(
	const uint32_t maxSets,
	const VkDescriptorPoolCreateFlags poolFlags,
	const std::vector<VkDescriptorPoolSize> &poolSizes,
	const VkDevice device)
{
	VkDescriptorPoolCreateInfo descriptorPoolInfo{};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolInfo.pPoolSizes = poolSizes.data();
	descriptorPoolInfo.maxSets = maxSets;
	descriptorPoolInfo.flags = poolFlags;

	if (vkCreateDescriptorPool(device,
		&descriptorPoolInfo,
		nullptr,
		&m_DescriptorPool) !=
		VK_SUCCESS)
	{
		FATAL_ERROR("failed to create descriptor pool!");
	}
}

VulkanDescriptorPool::~VulkanDescriptorPool()
{
	vkDestroyDescriptorPool(GetVkDevice()->GetDevice(), m_DescriptorPool, nullptr);
}

bool VulkanDescriptorPool::AllocateDescriptorSet(
	const VkDescriptorSetLayout& descriptorSetLayout,
	VkDescriptorSet &descriptor) const
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_DescriptorPool;
	allocInfo.pSetLayouts = &descriptorSetLayout;
	allocInfo.descriptorSetCount = 1;

	VulkanDevice::Lock lock;

	// Might want to create a "DescriptorPoolManager" class that handles this case, and builds
	// a new pool whenever an old pool fills up. But this is beyond our current scope
	if (vkAllocateDescriptorSets(GetVkDevice()->GetDevice(), &allocInfo, &descriptor) != VK_SUCCESS)
	{
		return false;
	}

	return true;
}

bool VulkanDescriptorPool::AllocateDescriptorSets(
	const VkDescriptorSetLayout& descriptorSetLayout,
	std::vector<VkDescriptorSet>& descriptors) const
{
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(descriptors.size());
	for (size_t i = 0; i < descriptors.size(); i++)
	{
		descriptorSetLayouts[i] = descriptorSetLayout;
	}

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_DescriptorPool;
	allocInfo.pSetLayouts = descriptorSetLayouts.data();
	allocInfo.descriptorSetCount = descriptors.size();

	VulkanDevice::Lock lock;

	// Might want to create a "DescriptorPoolManager" class that handles this case, and builds
	// a new pool whenever an old pool fills up. But this is beyond our current scope
	if (vkAllocateDescriptorSets(GetVkDevice()->GetDevice(), &allocInfo, descriptors.data()) != VK_SUCCESS)
	{
		return false;
	}

	return true;
}

void VulkanDescriptorPool::FreeDescriptors(const std::vector<VkDescriptorSet>& descriptors) const
{
	VulkanDevice::Lock lock;
	
	vkFreeDescriptorSets(
		GetVkDevice()->GetDevice(),
		m_DescriptorPool,
		static_cast<uint32_t>(descriptors.size()),
		descriptors.data());
}

void VulkanDescriptorPool::ResetPool() const
{
	vkDeviceWaitIdle(GetVkDevice()->GetDevice());

	vkResetDescriptorPool(GetVkDevice()->GetDevice(), m_DescriptorPool, 0);
}

VulkanDescriptorWriter::VulkanDescriptorWriter(VulkanDescriptorSetLayout &setLayout, VulkanDescriptorPool &pool)
	: m_SetLayout{setLayout}, m_Pool{pool} {}

VulkanDescriptorWriter &VulkanDescriptorWriter::WriteBuffer(
	const uint32_t binding, const VkDescriptorBufferInfo* bufferInfo)
{
	if (m_SetLayout.m_Bindings.count(binding) == 0)
	{
		FATAL_ERROR("Layout does not contain specified binding");
	}

	auto &bindingDescription = m_SetLayout.m_Bindings[binding];

	if (bindingDescription.descriptorCount != 1)
	{
		FATAL_ERROR("Binding single descriptor info, but binding expects multiple");
	}

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorType = bindingDescription.descriptorType;
	write.dstBinding = binding;
	write.pBufferInfo = bufferInfo;
	write.descriptorCount = 1;

	m_Writes.push_back(write);
	return *this;
}

VulkanDescriptorWriter & VulkanDescriptorWriter::WriteImage(
	const uint32_t binding, const VkDescriptorImageInfo* imageInfo)
{
	assert(m_SetLayout.m_Bindings.count(binding) == 1 && "Layout does not contain specified binding");

	auto &bindingDescription = m_SetLayout.m_Bindings[binding];

	assert(
		bindingDescription.descriptorCount == 1 &&
		"Binding single descriptor info, but binding expects multiple");

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorType = bindingDescription.descriptorType;
	write.dstBinding = binding;
	write.pImageInfo = imageInfo;
	write.descriptorCount = 1;

	m_Writes.push_back(write);
	return *this;
}

bool VulkanDescriptorWriter::Build(VkDescriptorSet &set)
{
	if (!m_Pool.AllocateDescriptorSet(m_SetLayout.GetDescriptorSetLayout(), set))
	{
		return false;
	}

	Overwrite(set);
	return true;
}

void VulkanDescriptorWriter::Overwrite(const VkDescriptorSet &set)
{
	for (auto& write : m_Writes)
	{
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(
		GetVkDevice()->GetDevice(),
		m_Writes.size(),
		m_Writes.data(),
		0,
		nullptr);
}