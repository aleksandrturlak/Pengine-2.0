#include "AccelerationStructure.h"

#include "../Vulkan/VulkanAccelerationStructure.h"
#include "../Core/Logger.h"

using namespace Pengine;

std::shared_ptr<AccelerationStructure> AccelerationStructure::CreateBLAS(const Mesh& mesh)
{
	if (graphicsAPI == GraphicsAPI::Vk)
	{
		return Vk::VulkanAccelerationStructure::CreateBLAS(mesh);
	}

	FATAL_ERROR("Failed to create acceleration structure (BLAS), no graphics API implementation");
	return nullptr;
}

std::shared_ptr<AccelerationStructure> Pengine::AccelerationStructure::CreateTLAS(const std::vector<Instance>& instances, void* frame)
{
    if (graphicsAPI == GraphicsAPI::Vk)
	{
		static_assert(sizeof(AccelerationStructure::Instance) == sizeof(VkAccelerationStructureInstanceKHR));
		return Vk::VulkanAccelerationStructure::CreateTLAS(instances, frame);
	}

	FATAL_ERROR("Failed to create acceleration structure (TLAS), no graphics API implementation");
	return nullptr;
}
