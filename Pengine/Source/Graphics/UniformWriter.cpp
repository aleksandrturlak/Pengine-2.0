#include "UniformWriter.h"

#include "../Core/Logger.h"
#include "../Core/TextureManager.h"
#include "../Utils/Utils.h"
#include "../Vulkan/VulkanUniformWriter.h"

using namespace Pengine;

std::shared_ptr<UniformWriter> UniformWriter::Create(
	std::shared_ptr<UniformLayout> uniformLayout,
	bool isMultiBuffered)
{
	if (graphicsAPI == GraphicsAPI::Vk)
	{
		return std::make_shared<Vk::VulkanUniformWriter>(uniformLayout, isMultiBuffered);
	}

	FATAL_ERROR("Failed to create the uniform writer, no graphics API implementation");
	return nullptr;
}

UniformWriter::UniformWriter(
	std::shared_ptr<UniformLayout> uniformLayout,
	bool isMultiBuffered)
	: m_UniformLayout(uniformLayout)
	, m_IsMultiBuffered(isMultiBuffered)
{
	m_Writes.resize(m_IsMultiBuffered ? Vk::frameInFlightCount : 1);
}

UniformWriter::~UniformWriter()
{
	m_Writes.clear();

	for (auto& [name, textures] : m_TexturesByName)
	{
		for (auto& texture : textures)
		{
			TextureManager::GetInstance().Delete(texture);
		}
	}
}

void UniformWriter::WriteBufferToFrame(
	uint32_t location,
	const std::shared_ptr<Buffer>& buffer,
	size_t size,
	size_t offset,
	uint32_t frameIndex)
{
	const auto binding = m_UniformLayout->GetBindingByLocation(location);
	if (!binding)
	{
		FATAL_ERROR("Layout does not contain specified binding!");
	}

	BufferWrite write{};
	write.buffers = { buffer };
	write.offset = offset;
	write.size = size;
	write.binding = *binding;

	std::lock_guard<std::mutex> lock(mutex);
	m_Writes[frameIndex].bufferWritesByLocation[location] = write;
}


void UniformWriter::WriteBufferToAllFrames(
	uint32_t location,
	const std::shared_ptr<Buffer>& buffer,
	size_t size,
	size_t offset)
{
	for (size_t i = 0; i < Vk::frameInFlightCount; i++)
	{
		WriteBufferToFrame(location, buffer, size, offset, i);
	}
}

void UniformWriter::WriteTextureToFrame(
	uint32_t location,
	const std::shared_ptr<Texture>& texture,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	WriteTexturesToFrame(location, { texture }, dstArrayElement, srcFrameIndex, dstFrameIndex);
}

void UniformWriter::WriteTexturesToFrame(
	uint32_t location,
	const std::vector<std::shared_ptr<Texture>>& textures,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	assert(!textures.empty());

	const auto binding = m_UniformLayout->GetBindingByLocation(location);
	if (!binding)
	{
		FATAL_ERROR("Layout does not contain specified binding!");
	}

	TextureWrite write{};
	write.textures = textures;
	write.binding = *binding;
	write.dstArrayElement = dstArrayElement;
	write.frameIndex = srcFrameIndex;

	std::lock_guard<std::mutex> lock(mutex);
	m_Writes[dstFrameIndex].textureWritesByLocation[location].emplace_back(write);
}

void UniformWriter::WriteTextureToAllFrames(
	uint32_t location,
	const std::shared_ptr<Texture>& texture,
	uint32_t dstArrayElement)
{
	WriteTexturesToAllFrames(location, { texture }, dstArrayElement);
}

void UniformWriter::WriteTexturesToAllFrames(
	uint32_t location,
	const std::vector<std::shared_ptr<Texture>>& textures,
	uint32_t dstArrayElement)
{
	for (size_t i = 0; i < Vk::frameInFlightCount; i++)
	{
		WriteTexturesToFrame(location, textures, dstArrayElement, i, i);
	}
}

void UniformWriter::WriteBufferToFrame(
	const std::string& name,
	const std::shared_ptr<Buffer>& buffer,
	size_t size,
	size_t offset,
	uint32_t frameIndex)
{
	const uint32_t location = m_UniformLayout->GetBindingLocationByName(name);
	m_BufferNameByLocation[location] = name;
	m_BuffersByName[name] = { buffer };

	WriteBufferToFrame(location, buffer, size, offset, frameIndex);
}

void UniformWriter::WriteBufferToAllFrames(
	const std::string& name,
	const std::shared_ptr<Buffer>& buffer,
	size_t size,
	size_t offset)
{
	for (size_t i = 0; i < Vk::frameInFlightCount; i++)
	{
		WriteBufferToFrame(name, buffer, size, offset, i);
	}
}

void UniformWriter::WriteTextureToFrame(
	const std::string& name,
	const std::shared_ptr<Texture>& texture,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	const uint32_t location = m_UniformLayout->GetBindingLocationByName(name);
	m_TextureNameByLocation[location] = name;
	m_TexturesByName[name] = { texture };

	WriteTextureToFrame(location, texture, dstArrayElement, srcFrameIndex, dstFrameIndex);
}

void UniformWriter::WriteTexturesToFrame(
	const std::string& name,
	const std::vector<std::shared_ptr<Texture>>& textures,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	const uint32_t location = m_UniformLayout->GetBindingLocationByName(name);
	m_TextureNameByLocation[location] = name;
	m_TexturesByName[name] = textures;

	WriteTexturesToFrame(location, textures, dstArrayElement, srcFrameIndex, dstFrameIndex);
}

void UniformWriter::WriteTextureToAllFrames(
	const std::string& name,
	const std::shared_ptr<Texture>& texture,
	uint32_t dstArrayElement)
{
	WriteTexturesToAllFrames(name, { texture }, dstArrayElement);
}

void UniformWriter::WriteTexturesToAllFrames(
	const std::string& name,
	const std::vector<std::shared_ptr<Texture>>& textures,
	uint32_t dstArrayElement)
{
	for (size_t i = 0; i < Vk::frameInFlightCount; i++)
	{
		WriteTexturesToFrame(name, textures, dstArrayElement, i, i);
	}
}

std::vector<std::shared_ptr<Buffer>> UniformWriter::GetBuffer(const std::string& name)
{
	return Utils::Find(name, m_BuffersByName);
}

std::vector<std::shared_ptr<Texture>> UniformWriter::GetTexture(const std::string& name)
{
	return Utils::Find(name, m_TexturesByName);
}
