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

	for (auto& [name, textureInfos] : m_TextureInfosByName)
	{
		for (auto& textureInfo : textureInfos)
		{
			TextureManager::GetInstance().Delete(textureInfo.texture);
		}
	}
}

void UniformWriter::WriteAccelerationStructureToFrame(
	uint32_t location,
	const std::shared_ptr<AccelerationStructure>& accelerationStructure,
	uint32_t frameIndex)
{
	const auto binding = m_UniformLayout->GetBindingByLocation(location);
	if (!binding)
	{
		FATAL_ERROR("Layout does not contain specified binding!");
	}

	AccelerationStructureWrite write{};
	write.accelerationStructures = { accelerationStructure };
	write.binding = *binding;

	std::lock_guard<std::mutex> lock(mutex);
	m_Writes[frameIndex].accelerationStructureWritesByLocation[location] = write;
}

void UniformWriter::WriteAccelerationStructureToAllFrames(
	uint32_t location,
	const std::shared_ptr<AccelerationStructure>& accelerationStructure)
{
	for (size_t i = 0; i < m_Writes.size(); i++)
	{
		WriteAccelerationStructureToFrame(location, accelerationStructure, i);
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
	for (size_t i = 0; i < m_Writes.size(); i++)
	{
		WriteBufferToFrame(location, buffer, size, offset, i);
	}
}

void UniformWriter::WriteTextureToFrame(
	uint32_t location,
	const TextureInfo& textureInfo,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	WriteTexturesToFrame(location, { textureInfo }, dstArrayElement, srcFrameIndex, dstFrameIndex);
}

void UniformWriter::WriteTexturesToFrame(
	uint32_t location,
	const std::vector<TextureInfo>& textureInfos,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	assert(!textureInfos.empty());

	const auto binding = m_UniformLayout->GetBindingByLocation(location);
	if (!binding)
	{
		FATAL_ERROR("Layout does not contain specified binding!");
	}

	TextureWrite write{};
	write.textureInfos = textureInfos;
	write.binding = *binding;
	write.dstArrayElement = dstArrayElement;
	write.frameIndex = srcFrameIndex;

	std::lock_guard<std::mutex> lock(mutex);
	m_Writes[dstFrameIndex].textureWritesByLocation[location].emplace_back(write);
}

void UniformWriter::WriteTextureToAllFrames(
	uint32_t location,
	const TextureInfo& textureInfo,
	uint32_t dstArrayElement)
{
	WriteTexturesToAllFrames(location, { textureInfo }, dstArrayElement);
}

void UniformWriter::WriteTexturesToAllFrames(
	uint32_t location,
	const std::vector<TextureInfo>& textureInfos,
	uint32_t dstArrayElement)
{
	for (size_t i = 0; i < m_Writes.size(); i++)
	{
		WriteTexturesToFrame(location, textureInfos, dstArrayElement, i, i);
	}
}

void UniformWriter::WriteAccelerationStructureToFrame(
	const std::string& name,
	const std::shared_ptr<AccelerationStructure>& accelerationStructure,
	uint32_t frameIndex)
{
	const uint32_t location = m_UniformLayout->GetBindingLocationByName(name);
	m_AccelerationStructureNameByLocation[location] = name;
	m_AccelerationStructuresByName[name] = { accelerationStructure };

	WriteAccelerationStructureToFrame(location, accelerationStructure, frameIndex);
}

void UniformWriter::WriteAccelerationStructureToAllFrames(
	const std::string& name,
	const std::shared_ptr<AccelerationStructure>& accelerationStructure)
{
	for (size_t i = 0; i < m_Writes.size(); i++)
	{
		WriteAccelerationStructureToFrame(name, accelerationStructure, i);
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
	for (size_t i = 0; i < m_Writes.size(); i++)
	{
		WriteBufferToFrame(name, buffer, size, offset, i);
	}
}

void UniformWriter::WriteTextureToFrame(
	const std::string& name,
	const TextureInfo& textureInfo,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	const uint32_t location = m_UniformLayout->GetBindingLocationByName(name);
	m_TextureNameByLocation[location] = name;
	m_TextureInfosByName[name] = { textureInfo };

	WriteTextureToFrame(location, { textureInfo }, dstArrayElement, srcFrameIndex, dstFrameIndex);
}

void UniformWriter::WriteTexturesToFrame(
	const std::string& name,
	const std::vector<TextureInfo>& textureInfos,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	const uint32_t location = m_UniformLayout->GetBindingLocationByName(name);
	m_TextureNameByLocation[location] = name;
	m_TextureInfosByName[name] = textureInfos;

	WriteTexturesToFrame(location, textureInfos, dstArrayElement, srcFrameIndex, dstFrameIndex);
}

void UniformWriter::WriteTextureToAllFrames(
	const std::string& name,
	const TextureInfo& textureInfo,
	uint32_t dstArrayElement)
{
	WriteTexturesToAllFrames(name, { textureInfo }, dstArrayElement);
}

void UniformWriter::WriteTexturesToAllFrames(
	const std::string& name,
	const std::vector<TextureInfo>& textureInfos,
	uint32_t dstArrayElement)
{
	for (size_t i = 0; i < m_Writes.size(); i++)
	{
		WriteTexturesToFrame(name, textureInfos, dstArrayElement, i, i);
	}
}

void UniformWriter::WriteTextureToFrame(
	const std::string& name,
	const std::shared_ptr<Texture>& texture,
	uint32_t dstArrayElement,
	uint32_t srcFrameIndex,
	uint32_t dstFrameIndex)
{
	WriteTextureToFrame(name, TextureInfo{ texture, 0 }, dstArrayElement, srcFrameIndex, dstFrameIndex);
}

void UniformWriter::WriteTextureToAllFrames(
	const std::string& name,
	const std::shared_ptr<Texture>& texture,
	uint32_t dstArrayElement)
{
	WriteTextureToAllFrames(name, TextureInfo{ texture, 0 }, dstArrayElement);
}

void UniformWriter::DeleteBuffer(const std::string& name)
{
	const uint32_t location = m_UniformLayout->GetBindingLocationByName(name);
	if (location != -1)
	{
		m_BuffersByName.erase(name);
		m_BufferNameByLocation.erase(location);
	}
}

void UniformWriter::DeleteTexture(const std::string &name)
{
	const uint32_t location = m_UniformLayout->GetBindingLocationByName(name);
	if (location != -1)
	{
		m_TextureInfosByName.erase(name);
		m_TextureNameByLocation.erase(location);
	}
}

std::vector<std::shared_ptr<Buffer>> UniformWriter::GetBuffer(const std::string& name)
{
	return Utils::Find(name, m_BuffersByName);
}

std::vector<UniformWriter::TextureInfo> UniformWriter::GetTextureInfo(const std::string& name)
{
	return Utils::Find(name, m_TextureInfosByName);
}
