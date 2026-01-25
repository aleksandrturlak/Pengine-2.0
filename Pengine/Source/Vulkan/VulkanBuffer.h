#pragma once

#include "../Core/Core.h"
#include "../Graphics/Buffer.h"

#include <vma/vk_mem_alloc.h>

namespace Pengine::Vk
{

	class PENGINE_API VulkanBuffer final : public Buffer
	{
	public:
		static std::shared_ptr<VulkanBuffer> Create(
			const size_t instanceSize,
			const uint32_t instanceCount,
			const std::vector<Usage>& usages,
			const MemoryType memoryType,
			const bool isMultiBuffered);

		static std::shared_ptr<VulkanBuffer> CreateStagingBuffer(
			VkDeviceSize instanceSize,
			uint32_t instanceCount);

		static VkBufferUsageFlagBits ConvertUsage(Usage usage);

		static Usage ConvertUsage(VkBufferUsageFlagBits usage);

		VulkanBuffer(
			const VkDeviceSize instanceSize,
			const uint32_t instanceCount,
			const VkBufferUsageFlags bufferUsageFlags,
			const VmaMemoryUsage memoryUsage,
			const VmaAllocationCreateFlags memoryFlags,
			const MemoryType memoryType,
			const bool isMultiBuffered,
			const VkDeviceSize minOffsetAlignment = 1);
		~VulkanBuffer() override;
		VulkanBuffer(const VkBuffer&) = delete;
		VulkanBuffer(VkBuffer&&) = delete;
		VulkanBuffer& operator=(const VkBuffer&) = delete;
		VulkanBuffer& operator=(VkBuffer&&) = delete;

		[[nodiscard]] virtual void* GetData() const override;

		virtual void WriteToBuffer(
			void* data,
			size_t size,
			size_t offset = 0) override;

		virtual void Copy(
			const std::shared_ptr<Buffer>& buffer,
			size_t dstOffset) override;

		virtual void Flush() override;

		[[nodiscard]] virtual size_t GetSize() const override { return m_BufferSize; }

		[[nodiscard]] virtual uint32_t GetInstanceCount() const override { return m_InstanceCount; }

		[[nodiscard]] virtual size_t GetInstanceSize() const override { return m_InstanceSize; }

		[[nodiscard]] virtual NativeHandle GetNativeHandle() const override;

		[[nodiscard]] virtual NativeHandle GetDeviceAddress() const override;
		
		[[nodiscard]] VkDescriptorBufferInfo GetDescriptorInfo(
			const uint32_t imageIndex,
			VkDeviceSize size = VK_WHOLE_SIZE,
			VkDeviceSize offset = 0) const;

		[[nodiscard]] inline VkBuffer GetBuffer() const { return m_BufferDatas[swapChainImageIndex * m_IsMultiBuffered].m_Buffer; }
		[[nodiscard]] inline VkDeviceSize GetAlignmentSize() const { return m_AlignmentSize; }
		[[nodiscard]] inline VkBufferUsageFlags GetUsageFlags() const { return m_UsageFlags; }
		[[nodiscard]] inline VmaMemoryUsage GetMemoryUsage() const { return m_MemoryUsage; }
		[[nodiscard]] inline VmaAllocationCreateFlags GetMemoryFlags() const { return m_MemoryFlags; }

	private:
		static VkDeviceSize GetAlignment(
			VkDeviceSize instanceSize,
			VkDeviceSize minOffsetAlignment);

		void WriteToVulkanBuffer(
			const uint32_t imageIndex,
			void* data,
			const size_t size,
			const size_t offset = 0);

		struct BufferData
		{
			VkBuffer m_Buffer = VK_NULL_HANDLE;
			VmaAllocation m_VmaAllocation = VK_NULL_HANDLE;
			VmaAllocationInfo m_VmaAllocationInfo{};
		};

		uint8_t* m_Data = nullptr;
		std::vector<BufferData> m_BufferDatas;

		VkBuffer m_CurrentBuffer = VK_NULL_HANDLE;

		VkDeviceSize m_BufferSize;
		uint32_t m_InstanceCount;
		VkDeviceSize m_InstanceSize;
		VkDeviceSize m_AlignmentSize;
		VkBufferUsageFlags m_UsageFlags;
		VmaMemoryUsage m_MemoryUsage;
		VmaAllocationCreateFlags m_MemoryFlags;

		std::vector<bool> m_IsChanged;
	};

}