#include "VulkanUniformWriter.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "VulkanUniformLayout.h"

#include "../Core/Logger.h"

using namespace Pengine;
using namespace Vk;

VulkanUniformWriter::VulkanUniformWriter(
	std::shared_ptr<UniformLayout> uniformLayout,
	bool isMultiBuffered)
	: UniformWriter(uniformLayout, isMultiBuffered)
{
	const size_t count = IsMultiBuffered() ? frameInFlightCount : 1;

	m_DescriptorSets.resize(count);

	if (!GetVkDevice()->GetDescriptorPool()->AllocateDescriptorSets(
		std::static_pointer_cast<VulkanUniformLayout>(m_UniformLayout)->GetDescriptorSetLayout(), m_DescriptorSets))
	{
		FATAL_ERROR("Failed to allocate descriptor set!");
	}
}

VulkanUniformWriter::~VulkanUniformWriter()
{
	GetVkDevice()->DeleteResource([descriptorSets = m_DescriptorSets]()
	{
		GetVkDevice()->GetDescriptorPool()->FreeDescriptors(descriptorSets);
	});
}

void VulkanUniformWriter::Flush()
{
	const uint32_t index = IsMultiBuffered() * frameInFlightIndex;
	const VkDescriptorSet set = m_DescriptorSets[index];

	std::vector<VkWriteDescriptorSet> writes;

	size_t bufferInfoCount = 0;
	for (const auto& [location, bufferWrite] : m_Writes[index].bufferWritesByLocation)
	{
		bufferInfoCount += bufferWrite.buffers.size();
	}

	std::vector<VkDescriptorBufferInfo> bufferInfos;
	bufferInfos.reserve(bufferInfoCount);
	for (const auto& [location, bufferWrite] : m_Writes[index].bufferWritesByLocation)
	{
		const size_t bufferInfoIndex = bufferInfos.size();

		for (const auto& buffer : bufferWrite.buffers)
		{
			assert(bufferInfoCount > 0);
			bufferInfos.emplace_back(std::static_pointer_cast<VulkanBuffer>(buffer)->GetDescriptorInfo(index, bufferWrite.size, bufferWrite.offset));
		}

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = VulkanUniformLayout::ConvertDescriptorType(bufferWrite.binding.type);
		write.dstBinding = location;
		write.pBufferInfo = &bufferInfos[bufferInfoIndex];
		write.descriptorCount = bufferWrite.binding.count;
		write.dstSet = set;

		writes.emplace_back(write);
	}
	m_Writes[index].bufferWritesByLocation.clear();

	size_t imageInfoCount = 0;
	for (const auto& [location, textureWrites] : m_Writes[index].textureWritesByLocation)
	{
		for (const auto& textureWrite : textureWrites)
		{
			imageInfoCount += textureWrite.textureInfos.size();
		}
	}

	std::vector<VkDescriptorImageInfo> imageInfos;
	imageInfos.reserve(imageInfoCount);
	for (const auto& [location, textureWrites] : m_Writes[index].textureWritesByLocation)
	{
		for (const auto& textureWrite : textureWrites)
		{
			const size_t imageInfoIndex = imageInfos.size();

			for (const auto& textureInfo : textureWrite.textureInfos)
			{
				assert(imageInfoCount > 0);
				imageInfos.emplace_back(std::static_pointer_cast<VulkanTexture>(textureInfo.texture)->GetDescriptorInfo(textureInfo.baseMipLevel, textureWrite.frameIndex));
			}

			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = VulkanUniformLayout::ConvertDescriptorType(textureWrite.binding.type);
			write.dstBinding = location;
			write.pImageInfo = &imageInfos[imageInfoIndex];
			write.descriptorCount = textureWrite.textureInfos.size();
			write.dstArrayElement = textureWrite.dstArrayElement;
			write.dstSet = set;

			writes.emplace_back(write);
		}
	}
	m_Writes[index].textureWritesByLocation.clear();

	if (writes.empty())
	{
		return;
	}

	vkUpdateDescriptorSets(GetVkDevice()->GetDevice(), writes.size(), writes.data(), 0, nullptr);
}

NativeHandle VulkanUniformWriter::GetNativeHandle() const
{
	return NativeHandle((size_t)GetDescriptorSet());
}

void VulkanUniformWriter::WriteTexture(uint32_t location, const std::vector<VkDescriptorImageInfo>& vkDescriptorImageInfos)
{
	const size_t count = IsMultiBuffered() ? frameInFlightCount : 1;
	assert(count == vkDescriptorImageInfos.size());

	const auto binding = m_UniformLayout->GetBindingByLocation(location);

	if (!binding)
	{
		FATAL_ERROR("Layout does not contain specified binding!");
	}

	std::vector<VkWriteDescriptorSet> writes;
	writes.resize(count);
	for (size_t i = 0; i < count; i++)
	{
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = VulkanUniformLayout::ConvertDescriptorType(binding->type);
		write.dstBinding = location;
		write.pImageInfo = &vkDescriptorImageInfos[i];
		write.descriptorCount = binding->count;
		write.dstSet = m_DescriptorSets[i];

		writes[i] = write;
	}

	vkUpdateDescriptorSets(GetVkDevice()->GetDevice(), writes.size(), writes.data(), 0, nullptr);
}

VkDescriptorSet VulkanUniformWriter::GetDescriptorSet(const uint32_t frameIndex) const
{
	return m_DescriptorSets[IsMultiBuffered() * frameIndex];
}
