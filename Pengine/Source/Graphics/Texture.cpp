#include "Texture.h"

#include "../Core/Logger.h"
#include "../Core/Profiler.h"
#include "../Vulkan/VulkanTexture.h"
#include "../Utils/Utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stbi/stb_image.h>

using namespace Pengine;

std::shared_ptr<Texture> Texture::Create(const CreateInfo& createInfo)
{
	PROFILER_SCOPE(__FUNCTION__);

	if (graphicsAPI == GraphicsAPI::Vk)
	{
		return std::make_shared<Vk::VulkanTexture>(createInfo);
	}

	FATAL_ERROR("Failed to create the texture, no graphics API implementation");
	return nullptr;
}

std::shared_ptr<Texture> Texture::Load(const std::filesystem::path& filepath, bool flip, const Meta& meta)
{
	PROFILER_SCOPE(__FUNCTION__);

	stbi_set_flip_vertically_on_load(flip);

	CreateInfo textureCreateInfo{};
	textureCreateInfo.meta = meta;
	textureCreateInfo.name = Utils::GetFilename(filepath);
	textureCreateInfo.filepath = filepath;
	textureCreateInfo.aspectMask = AspectMask::COLOR;

	textureCreateInfo.format = meta.srgb ? Format::R8G8B8A8_SRGB : Format::R8G8B8A8_UNORM;

	int channels = 0;
	void* data = stbi_load(
		filepath.string().c_str(),
		&textureCreateInfo.size.x,
		&textureCreateInfo.size.y,
		&channels,
		STBI_rgb_alpha);
	textureCreateInfo.instanceSize = sizeof(uint8_t) * STBI_rgb_alpha;

	const size_t size = textureCreateInfo.size.x * textureCreateInfo.size.y * textureCreateInfo.instanceSize;
	textureCreateInfo.data = data;

	textureCreateInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(
		textureCreateInfo.size.x, textureCreateInfo.size.y)))) + 1;
	textureCreateInfo.usage = { Usage::SAMPLED, Usage::TRANSFER_DST, Usage::TRANSFER_SRC };

	if (!textureCreateInfo.data)
	{
		Logger::Error("Texture can't be loaded from the filepath " + filepath.string());
		return nullptr;
	}

	Logger::Log("Texture:" + textureCreateInfo.filepath.string() + " has been loaded!", BOLDGREEN);

	std::shared_ptr<Texture> texture;
	if (graphicsAPI == GraphicsAPI::Vk)
	{
		texture = std::make_shared<Vk::VulkanTexture>(textureCreateInfo);
	}

	stbi_image_free(data);

	if (!texture)
	{
		FATAL_ERROR("Failed to create the renderer, no graphics API implementation");
		return nullptr;
	}

	return texture;
}

Texture::Texture(const CreateInfo& createInfo)
	: Asset(createInfo.name, createInfo.filepath)
{
	m_Meta = createInfo.meta;
	m_Size = createInfo.size;
	m_Format = createInfo.format;
	m_AspectMask = createInfo.aspectMask;
	m_MipLevels = createInfo.mipLevels;
	m_LayerCount = createInfo.isCubeMap ? 6 : createInfo.layerCount;
	m_InstanceSize = createInfo.instanceSize;
	m_IsMultiBuffered = createInfo.isMultiBuffered;
	m_MemoryType = createInfo.memoryType;
}
