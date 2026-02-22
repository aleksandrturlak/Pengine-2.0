#include "VulkanBuffer.h"

#include "VulkanDevice.h"

#include "../Core/Logger.h"

using namespace Pengine;
using namespace Vk;

VkDeviceSize VulkanBuffer::GetAlignment(const VkDeviceSize instanceSize, const VkDeviceSize minOffsetAlignment)
{
	if (minOffsetAlignment > 0)
	{
		return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
	}

	return instanceSize;
}

void VulkanBuffer::WriteToVulkanBuffer(
	const uint32_t frameIndex,
	void* data,
	const size_t size,
	const size_t offset)
{
	if (GetMemoryType() == MemoryType::CPU)
	{
		vmaCopyMemoryToAllocation(GetVkDevice()->GetVmaAllocator(), data, m_BufferDatas[frameIndex].m_VmaAllocation, offset, size);
	}
	else if (GetMemoryType() == MemoryType::GPU)
	{
		const std::shared_ptr<VulkanBuffer> stagingBuffer = CreateStagingBuffer(
			size,
			1);

		stagingBuffer->WriteToBuffer(data, size, offset);

		GetVkDevice()->CopyBuffer(
			stagingBuffer->m_BufferDatas[frameIndex].m_Buffer,
			m_BufferDatas[frameIndex].m_Buffer,
			size,
			offset);
	}
}

std::shared_ptr<VulkanBuffer> VulkanBuffer::Create(const CreateInfo& createInfo)
{
	return std::make_shared<VulkanBuffer>(createInfo);
}

std::shared_ptr<VulkanBuffer> VulkanBuffer::CreateStagingBuffer(
	const VkDeviceSize instanceSize,
	const uint32_t instanceCount)
{
	Buffer::CreateInfo createInfo{};
	createInfo.instanceSize = instanceSize;
	createInfo.instanceCount = instanceCount;
	createInfo.usages = { Buffer::Usage::TRANSFER_SRC };
	createInfo.memoryType = MemoryType::CPU;
	createInfo.isMultiBuffered = false;
	return std::make_shared<VulkanBuffer>(createInfo);
}

VkBufferUsageFlagBits VulkanBuffer::ConvertUsage(const Usage usage)
{
	switch (usage)
	{
	case Pengine::Buffer::Usage::UNIFORM_BUFFER:
		return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	case Pengine::Buffer::Usage::VERTEX_BUFFER:
		return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	case Pengine::Buffer::Usage::INDEX_BUFFER:
		return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	case Pengine::Buffer::Usage::STORAGE_BUFFER:
		return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	case Pengine::Buffer::Usage::INDIRECT_BUFFER:
		return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	case Pengine::Buffer::Usage::TRANSFER_SRC:
		return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	case Pengine::Buffer::Usage::TRANSFER_DST:
		return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	FATAL_ERROR("Failed to convert buffer usage!");
}

Buffer::Usage VulkanBuffer::ConvertUsage(const VkBufferUsageFlagBits usage)
{
	switch (usage)
	{
	case VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT:
		return Pengine::Buffer::Usage::UNIFORM_BUFFER;
	case VK_BUFFER_USAGE_VERTEX_BUFFER_BIT:
		return Pengine::Buffer::Usage::VERTEX_BUFFER;
	case VK_BUFFER_USAGE_INDEX_BUFFER_BIT:
		return Pengine::Buffer::Usage::INDEX_BUFFER;
	case VK_BUFFER_USAGE_STORAGE_BUFFER_BIT:
		return Pengine::Buffer::Usage::STORAGE_BUFFER;
	case VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT:
		return Pengine::Buffer::Usage::INDIRECT_BUFFER;
	case VK_BUFFER_USAGE_TRANSFER_SRC_BIT:
		return Pengine::Buffer::Usage::TRANSFER_SRC;
	case VK_BUFFER_USAGE_TRANSFER_DST_BIT:
		return Pengine::Buffer::Usage::TRANSFER_DST;
	}

	FATAL_ERROR("Failed to convert buffer usage!");
	return {};
}

VulkanBuffer::VulkanBuffer(const CreateInfo& createInfo)
	: Buffer(createInfo)
	, m_InstanceCount(createInfo.instanceCount)
	, m_InstanceSize(createInfo.instanceSize)
{
	m_UsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	for (const Usage& usage : createInfo.usages)
	{
		m_UsageFlags |= ConvertUsage(usage);
	}
	

	if (createInfo.memoryType == MemoryType::CPU)
	{
		m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		m_MemoryFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}
	else if (createInfo.memoryType == MemoryType::GPU)
	{
		m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	}

	m_AlignmentSize = GetAlignment(createInfo.instanceSize, 1);
	m_BufferSize = m_AlignmentSize * createInfo.instanceCount;

	if (createInfo.isMultiBuffered)
	{
		m_Data = new uint8_t[m_BufferSize];
	}

	m_IsChanged.resize(createInfo.isMultiBuffered ? frameInFlightCount : 1, false);
	m_BufferDatas.resize(createInfo.isMultiBuffered ? frameInFlightCount : 1);
	for (BufferData& bufferData : m_BufferDatas)
	{
		GetVkDevice()->CreateBuffer(
			m_BufferSize,
			m_UsageFlags,
			m_MemoryUsage,
			m_MemoryFlags,
			bufferData.m_Buffer,
			bufferData.m_VmaAllocation,
			bufferData.m_VmaAllocationInfo);

		VkBufferDeviceAddressInfo addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addressInfo.buffer = bufferData.m_Buffer;

		bufferData.m_DeviceAddress = vkGetBufferDeviceAddress(GetVkDevice()->GetDevice(), &addressInfo);
	}
}

VulkanBuffer::~VulkanBuffer()
{
	for (const BufferData& bufferData : m_BufferDatas)
	{
		GetVkDevice()->DestroyBuffer(bufferData.m_Buffer, bufferData.m_VmaAllocation, bufferData.m_VmaAllocationInfo);
	}

	if (m_Data)
	{
		delete[] m_Data;
		m_Data = nullptr;
	}
}

void* VulkanBuffer::GetData() const
{
	if (IsMultiBuffered())
	{
		return m_Data;
	}

	// TODO: Make possible get data from GPU.
	if (GetMemoryType() != MemoryType::CPU)
	{
		FATAL_ERROR("Can't get data from the buffer that is allocated on GPU!");
	}

	return m_BufferDatas[frameInFlightIndex].m_VmaAllocationInfo.pMappedData;
}

void VulkanBuffer::WriteToBuffer(void* data, const size_t size, const size_t offset)
{
	if (IsMultiBuffered())
	{
		m_IsChanged.assign(m_IsChanged.size(), true);
		memcpy((void*)&m_Data[offset], data, size);
	}
	else
	{
		WriteToVulkanBuffer(0, data, size, offset);
	}
}

void VulkanBuffer::Copy(
	const std::shared_ptr<Buffer>& buffer,
	const size_t dstOffset)
{
	const std::shared_ptr<VulkanBuffer> vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);

	if (GetSize() < vkBuffer->GetSize())
	{
		Logger::Error("Can't copy buffer, src size is bigger than dst size!");
	}

	m_IsChanged.assign(m_IsChanged.size(), true);

	if (IsMultiBuffered())
	{
		memcpy((void*)m_Data[dstOffset], vkBuffer->m_Data, GetSize());
	}
	else
	{
		GetVkDevice()->CopyBuffer(
			vkBuffer->GetBuffer(),
			m_BufferDatas.back().m_Buffer,
			vkBuffer->GetSize(),
			0,
			dstOffset);
	}
}

void VulkanBuffer::Flush()
{
	if (!IsMultiBuffered())
	{
		return;
	}

	const uint32_t frameIndex = m_IsChanged.size() == 1 ? 0 : frameInFlightIndex;

	if (!m_IsChanged[frameIndex])
	{
		return;
	}

	// TODO: can be done more optimal by coping just updated parts of the buffer.
	if (GetMemoryType() == MemoryType::CPU)
	{
		vmaCopyMemoryToAllocation(
			GetVkDevice()->GetVmaAllocator(),
			m_Data,
			m_BufferDatas[frameIndex].m_VmaAllocation,
			0,
			GetSize());
	}
	else if (GetMemoryType() == MemoryType::GPU)
	{
		const std::shared_ptr<VulkanBuffer> stagingBuffer = CreateStagingBuffer(
			GetSize(),
			1);

		stagingBuffer->WriteToBuffer(m_Data, GetSize(), 0);

		GetVkDevice()->CopyBuffer(
			stagingBuffer->m_BufferDatas.begin()->m_Buffer,
			m_BufferDatas[frameIndex].m_Buffer,
			GetSize(),
			0);
	}

	m_IsChanged[frameIndex] = false;
}

void VulkanBuffer::ClearWrites()
{
	m_IsChanged.assign(m_IsChanged.size(), false);
}

VkDescriptorBufferInfo VulkanBuffer::GetDescriptorInfo(
	const uint32_t frameIndex,
	const VkDeviceSize size,
	const VkDeviceSize offset) const
{
	return VkDescriptorBufferInfo{
		m_BufferDatas[frameIndex].m_Buffer,
		offset,
		size,
	};
}
