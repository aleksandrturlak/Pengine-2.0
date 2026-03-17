#include "Buffer.h"

#include "../Vulkan/VulkanBuffer.h"
#include "../Core/Logger.h"

using namespace Pengine;

std::shared_ptr<Buffer> Buffer::Create(const CreateInfo& createInfo)
{
	if (graphicsAPI == GraphicsAPI::Vk)
	{
		return Vk::VulkanBuffer::Create(createInfo);
	}

	FATAL_ERROR("Failed to create the buffer, no graphics API implementation");
	return nullptr;
}

Buffer::Buffer(const CreateInfo& createInfo)
	: m_CreateInfo(createInfo)
{
}