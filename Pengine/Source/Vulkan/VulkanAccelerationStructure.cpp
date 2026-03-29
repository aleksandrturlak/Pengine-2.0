#include "VulkanAccelerationStructure.h"

#include "VulkanDevice.h"

#include "../Core/Logger.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/Vertex.h"

using namespace Pengine;
using namespace Vk;

VkBuffer CreateScratchBuffer(const VkDeviceSize size, VkDeviceAddress& deviceAddress)
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationInfo allocationInfo{};

	GetVkDevice()->CreateBuffer(
		size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		0,
		buffer,
		allocation,
		allocationInfo,
		GetVkDevice()->GetMinScratchOffsetAlignment());

	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = buffer;
	deviceAddress = vkGetBufferDeviceAddress(GetVkDevice()->GetDevice(), &addressInfo);

	GetVkDevice()->DeleteResource([buffer, allocation, size = allocationInfo.size]()
	{
		vramAllocated -= size;
		vmaDestroyBuffer(GetVkDevice()->GetVmaAllocator(), buffer, allocation);
	});

	return buffer;
}

VulkanAccelerationStructure::~VulkanAccelerationStructure()
{
	if (m_AccelerationStructure != VK_NULL_HANDLE)
	{
		const VkAccelerationStructureKHR as = m_AccelerationStructure;
		const VkBuffer buffer = m_Buffer;
		const VmaAllocation allocation = m_Allocation;
		const VmaAllocationInfo allocationInfo = m_AllocationInfo;

		GetVkDevice()->DeleteResource([as, buffer, allocation, size = allocationInfo.size]()
		{
			GetVkDevice()->DestroyAccelerationStructure(as);
			vramAllocated -= size;
			vmaDestroyBuffer(GetVkDevice()->GetVmaAllocator(), buffer, allocation);
		});
	}
}

std::shared_ptr<VulkanAccelerationStructure> VulkanAccelerationStructure::CreateBLAS(const Mesh& mesh)
{
	if (mesh.GetVertexCount() == 0 || mesh.GetIndexCount() == 0)
	{
		return nullptr;
	}

	const VkDeviceAddress vertexAddress = mesh.GetVertexBuffer(0)->GetDeviceAddress().Get();
	const VkDeviceAddress indexAddress = mesh.GetIndexBuffer()->GetDeviceAddress().Get();

	const uint32_t primitiveCount = static_cast<uint32_t>(mesh.GetLods()[0].indexCount / 3);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vertexAddress;
	triangles.vertexStride = mesh.GetVertexLayouts()[0].size;
	triangles.maxVertex = static_cast<uint32_t>(mesh.GetVertexCount() - 1);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	triangles.transformData = {};

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;
	geometry.flags = 0;

	VkAccelerationStructureBuildRangeInfoKHR buildRange{};
	buildRange.primitiveCount = primitiveCount;
	buildRange.primitiveOffset = static_cast<uint32_t>(mesh.GetLods()[0].indexOffset * sizeof(uint32_t));
	buildRange.firstVertex = 0;
	buildRange.transformOffset = 0;

	VkCommandBuffer commandBuffer = GetVkDevice()->BeginSingleTimeCommands();
	auto blas = Build(
		VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		{ geometry },
		{ buildRange },
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		commandBuffer);
	GetVkDevice()->EndSingleTimeCommands(commandBuffer);

	return blas;
}

std::shared_ptr<VulkanAccelerationStructure> VulkanAccelerationStructure::CreateSkinnedBLAS(
	const Mesh& mesh,
	const uint64_t skinnedVertexAddress,
	void* frame)
{
	if (mesh.GetVertexCount() == 0 || mesh.GetIndexCount() == 0)
	{
		return nullptr;
	}

	const VkDeviceAddress indexAddress = mesh.GetIndexBuffer()->GetDeviceAddress().Get();
	const uint32_t primitiveCount = static_cast<uint32_t>(mesh.GetLods()[0].indexCount / 3);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = skinnedVertexAddress;
	triangles.vertexStride = sizeof(VertexSkinnedOutput);
	triangles.maxVertex = static_cast<uint32_t>(mesh.GetVertexCount() - 1);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	triangles.transformData = {};

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;
	geometry.flags = 0;

	VkAccelerationStructureBuildRangeInfoKHR buildRange{};
	buildRange.primitiveCount = primitiveCount;
	buildRange.primitiveOffset = static_cast<uint32_t>(mesh.GetLods()[0].indexOffset * sizeof(uint32_t));
	buildRange.firstVertex = 0;
	buildRange.transformOffset = 0;

	const std::shared_ptr<VulkanDevice> vkDevice = std::static_pointer_cast<VulkanDevice>(device);
	VkCommandBuffer commandBuffer = vkDevice->GetCommandBufferFromFrame(frame);

	return Build(
		VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		{ geometry },
		{ buildRange },
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR |
		VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
		commandBuffer);
}

void VulkanAccelerationStructure::Rebuild(
	const Mesh& mesh,
	const uint64_t skinnedVertexAddress,
	void* frame)
{
	if (m_AccelerationStructure == VK_NULL_HANDLE)
		return;

	const VkDeviceAddress indexAddress = mesh.GetIndexBuffer()->GetDeviceAddress().Get();
	const uint32_t primitiveCount = static_cast<uint32_t>(mesh.GetLods()[0].indexCount / 3);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = skinnedVertexAddress;
	triangles.vertexStride = sizeof(VertexSkinnedOutput);
	triangles.maxVertex = static_cast<uint32_t>(mesh.GetVertexCount() - 1);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	triangles.transformData = {};

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;
	geometry.flags = 0;

	uint32_t maxPrimitiveCount = primitiveCount;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR |
	                  VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
	buildInfo.srcAccelerationStructure = m_AccelerationStructure;
	buildInfo.dstAccelerationStructure = m_AccelerationStructure;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
	GetVkDevice()->GetAccelerationStructureBuildSizes(
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		buildInfo,
		&maxPrimitiveCount,
		sizeInfo);

	VkDeviceAddress scratchAddress{};
	CreateScratchBuffer(sizeInfo.updateScratchSize, scratchAddress);
	buildInfo.scratchData.deviceAddress = scratchAddress;

	VkAccelerationStructureBuildRangeInfoKHR buildRange{};
	buildRange.primitiveCount = primitiveCount;
	buildRange.primitiveOffset = static_cast<uint32_t>(mesh.GetLods()[0].indexOffset * sizeof(uint32_t));
	buildRange.firstVertex = 0;
	buildRange.transformOffset = 0;

	const std::shared_ptr<VulkanDevice> vkDevice = std::static_pointer_cast<VulkanDevice>(device);
	VkCommandBuffer commandBuffer = vkDevice->GetCommandBufferFromFrame(frame);

	const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &buildRange;
	vkDevice->CmdBuildAccelerationStructures(commandBuffer, 1, &buildInfo, &pBuildRange);

	VkMemoryBarrier memBarrier{};
	memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	memBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0, 1, &memBarrier, 0, nullptr, 0, nullptr);
}

std::shared_ptr<VulkanAccelerationStructure> VulkanAccelerationStructure::CreateTLAS(
	const std::vector<AccelerationStructure::Instance>& instances,
	void* frame)
{
	if (instances.empty())
	{
		return nullptr;
	}

	const std::shared_ptr<VulkanDevice> vkDevice = std::static_pointer_cast<VulkanDevice>(device);
	const VkCommandBuffer commandBuffer = vkDevice->GetCommandBufferFromFrame(frame);

	vkDevice->CommandBeginLabel("BuildTLAS", commandBuffer, { 0.5f, 1.0f, 0.5f });

	const VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();

	VkBuffer instanceBuffer = VK_NULL_HANDLE;
	VmaAllocation instanceAllocation = VK_NULL_HANDLE;
	VmaAllocationInfo instanceAllocationInfo{};

	vkDevice->CreateBuffer(
		instanceBufferSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		0,
		instanceBuffer,
		instanceAllocation,
		instanceAllocationInfo);

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;
	VmaAllocationInfo stagingAllocationInfo{};

	vkDevice->CreateBuffer(
		instanceBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
		VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		stagingBuffer,
		stagingAllocation,
		stagingAllocationInfo);

	vmaCopyMemoryToAllocation(
		vkDevice->GetVmaAllocator(),
		instances.data(),
		stagingAllocation,
		0,
		instanceBufferSize);

	vkDevice->CopyBuffer(commandBuffer, stagingBuffer, instanceBuffer, instanceBufferSize);

	vkDevice->DeleteResource([stagingBuffer, stagingAllocation, size = stagingAllocationInfo.size]()
	{
		vramAllocated -= size;
		vmaDestroyBuffer(GetVkDevice()->GetVmaAllocator(), stagingBuffer, stagingAllocation);
	});

	// Barrier: ensure instance data writes are visible before AS build
	VkMemoryBarrier barrier{};
	barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0, 1, &barrier, 0, nullptr, 0, nullptr);

	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = instanceBuffer;
	const VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddress(vkDevice->GetDevice(), &addressInfo);

	vkDevice->DeleteResource([instanceBuffer, instanceAllocation, size = instanceAllocationInfo.size]()
	{
		vramAllocated -= size;
		vmaDestroyBuffer(GetVkDevice()->GetVmaAllocator(), instanceBuffer, instanceAllocation);
	});

	VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
	instancesData.sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instancesData.arrayOfPointers = VK_FALSE;
	instancesData.data.deviceAddress = instanceBufferAddress;

	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType          = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances    = instancesData;

	VkAccelerationStructureBuildRangeInfoKHR buildRange{};
	buildRange.primitiveCount  = static_cast<uint32_t>(instances.size());
	buildRange.primitiveOffset = 0;
	buildRange.firstVertex     = 0;
	buildRange.transformOffset = 0;

	const auto tlas = Build(
		VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		{ geometry },
		{ buildRange },
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR |
		VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
		commandBuffer);

	vkDevice->CommandEndLabel(commandBuffer);

	return tlas;
}

std::shared_ptr<VulkanAccelerationStructure> VulkanAccelerationStructure::Build(
	const VkAccelerationStructureTypeKHR type,
	const std::vector<VkAccelerationStructureGeometryKHR>& geometries,
	const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& buildRanges,
	const VkBuildAccelerationStructureFlagsKHR flags,
	VkCommandBuffer commandBuffer)
{
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type          = type;
	buildInfo.flags         = flags;
	buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.geometryCount = static_cast<uint32_t>(geometries.size());
	buildInfo.pGeometries   = geometries.data();

	std::vector<uint32_t> maxPrimitiveCounts(buildRanges.size());
	for (size_t i = 0; i < buildRanges.size(); ++i)
	{
		maxPrimitiveCounts[i] = buildRanges[i].primitiveCount;
	}

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
	GetVkDevice()->GetAccelerationStructureBuildSizes(
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		buildInfo,
		maxPrimitiveCounts.data(),
		sizeInfo);

	// Allocate backing storage buffer
	VkBuffer asBuffer = VK_NULL_HANDLE;
	VmaAllocation asAllocation = VK_NULL_HANDLE;
	VmaAllocationInfo asAllocationInfo{};

	GetVkDevice()->CreateBuffer(
		sizeInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		0,
		asBuffer,
		asAllocation,
		asAllocationInfo);

	// Create acceleration structure
	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.buffer = asBuffer;
	createInfo.size   = sizeInfo.accelerationStructureSize;
	createInfo.type   = type;

	VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
	GetVkDevice()->CreateAccelerationStructure(createInfo, accelerationStructure);

	// Allocate scratch buffer (deferred deletion after GPU execution)
	VkDeviceAddress scratchAddress{};
	CreateScratchBuffer(sizeInfo.buildScratchSize, scratchAddress);

	// Record build
	buildInfo.dstAccelerationStructure  = accelerationStructure;
	buildInfo.scratchData.deviceAddress = scratchAddress;

	const VkAccelerationStructureBuildRangeInfoKHR* pBuildRanges = buildRanges.data();
	GetVkDevice()->CmdBuildAccelerationStructures(commandBuffer, 1, &buildInfo, &pBuildRanges);

	// Pipeline barrier: AS build writes → AS reads
	VkMemoryBarrier memBarrier{};
	memBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	memBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, nullptr, 0, nullptr);

	// Package result
	auto result = std::make_shared<VulkanAccelerationStructure>();
	result->m_AccelerationStructure = accelerationStructure;
	result->m_Buffer     = asBuffer;
	result->m_Allocation = asAllocation;
	result->m_AllocationInfo = asAllocationInfo;
	result->m_DeviceAddress  = GetVkDevice()->GetAccelerationStructureDeviceAddress(accelerationStructure);

	return result;
}
