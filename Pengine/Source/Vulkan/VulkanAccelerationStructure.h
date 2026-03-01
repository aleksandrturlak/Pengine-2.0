#pragma once

#include "../Core/Core.h"
#include "../Graphics/AccelerationStructure.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vma/vk_mem_alloc.h>

namespace Pengine
{
	class Mesh;
}

namespace Pengine::Vk
{

	class PENGINE_API VulkanAccelerationStructure final : public AccelerationStructure
	{
	public:
		static std::shared_ptr<VulkanAccelerationStructure> CreateBLAS(const Mesh& mesh);

		static std::shared_ptr<VulkanAccelerationStructure> CreateTLAS(
			const std::vector<AccelerationStructure::Instance>& instances,
			void* frame);

		VulkanAccelerationStructure() = default;
		~VulkanAccelerationStructure() override;
		VulkanAccelerationStructure(const VulkanAccelerationStructure&) = delete;
		VulkanAccelerationStructure& operator=(const VulkanAccelerationStructure&) = delete;

		[[nodiscard]] virtual uint64_t GetDeviceAddress() const override { return m_DeviceAddress; }

		[[nodiscard]] VkAccelerationStructureKHR GetHandle() const { return m_AccelerationStructure; }

	private:
		static std::shared_ptr<VulkanAccelerationStructure> Build(
			VkAccelerationStructureTypeKHR type,
			const std::vector<VkAccelerationStructureGeometryKHR>& geometries,
			const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& buildRanges,
			VkBuildAccelerationStructureFlagsKHR flags,
			VkCommandBuffer commandBuffer);

		VkAccelerationStructureKHR m_AccelerationStructure = VK_NULL_HANDLE;
		VkBuffer      m_Buffer     = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;
		VmaAllocationInfo m_AllocationInfo{};
		uint64_t m_DeviceAddress{};
	};

}
