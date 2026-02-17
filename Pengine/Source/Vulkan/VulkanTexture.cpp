#include "VulkanTexture.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanDevice.h"
#include "VulkanFormat.h"
#include "VulkanSamplerManager.h"
#include "VulkanUniformLayout.h"
#include "VulkanUniformWriter.h"

#include "../Core/Logger.h"
#include "../Graphics/RenderPass.h"

#include <imgui/backends/imgui_impl_vulkan.h>

using namespace Pengine;
using namespace Vk;

VulkanTexture::VulkanTexture(const CreateInfo& createInfo)
	: Texture(createInfo)
{
	VkFormat format = ConvertFormat(m_Format);
	VkImageAspectFlagBits aspectMask = ConvertAspectMask(m_AspectMask);

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(m_Size.x);
	imageInfo.extent.height = static_cast<uint32_t>(m_Size.y);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = m_MipLevels;
	imageInfo.arrayLayers = m_LayerCount;
	imageInfo.format = format;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = createInfo.isCubeMap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	for (Usage usage : createInfo.usage)
	{
		imageInfo.usage |= ConvertUsage(usage);
	}

	if (m_IsMultiBuffered)
	{
		m_ImageDatas.resize(Vk::frameInFlightCount);
	}
	else
	{
		m_ImageDatas.resize(1);
	}

	VmaMemoryUsage memoryUsage{};
	VmaAllocationCreateFlags memoryFlags{};
	if (createInfo.memoryType == MemoryType::GPU)
	{
		memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (createInfo.memoryType == MemoryType::CPU)
	{
		memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		memoryFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	}

	for (auto& imageData : m_ImageDatas)
	{
		GetVkDevice()->CreateImage(
			imageInfo,
			memoryUsage,
			memoryFlags,
			imageData.image,
			imageData.vmaAllocation,
			imageData.vmaAllocationInfo);

		TransitionInternal(imageData, VK_IMAGE_LAYOUT_GENERAL);
	}

	if (createInfo.data)
	{
		std::shared_ptr<VulkanBuffer> stagingBuffer = VulkanBuffer::CreateStagingBuffer(
			createInfo.instanceSize,
			m_Size.x * m_Size.y);

		stagingBuffer->WriteToBuffer(createInfo.data, stagingBuffer->GetSize());

		for (auto& imageData : m_ImageDatas)
		{
			GetVkDevice()->CopyBufferToImage(
				stagingBuffer->GetBuffer(),
				imageData.image,
				static_cast<uint32_t>(m_Size.x),
				static_cast<uint32_t>(m_Size.y));

			if (m_MipLevels > 1)
			{
				GetVkDevice()->GenerateMipMaps(
					imageData.image,
					ConvertFormat(m_Format),
					m_Size.x,
					m_Size.y,
					m_MipLevels,
					m_LayerCount,
					VK_NULL_HANDLE);
			}
		}
	}

	VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;
	if (createInfo.isCubeMap)
	{
		imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
	}
	else if (createInfo.layerCount > 1)
	{
		imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	}

	for (auto& imageData : m_ImageDatas)
	{
		for (size_t i = 0; i < m_MipLevels; i++)
		{
			imageData.views.emplace_back(CreateImageView(
				imageData.image,
				format,
				aspectMask,
				i,
				m_MipLevels - i,
				m_LayerCount,
				imageViewType));
		}
	}

	m_Sampler = CreateSampler(createInfo.samplerCreateInfo);

	// Setup uniform writer. Maybe need to refactor.
	{
		std::vector<ShaderReflection::ReflectDescriptorSetBinding> bindings;
		ShaderReflection::ReflectDescriptorSetBinding& binding = bindings.emplace_back();
		binding.name = "imageTexture";
		binding.binding = 0;
		binding.count = 1;

		if ((imageInfo.usage & VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT) == VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT)
		{
			binding.type = ShaderReflection::Type::STORAGE_IMAGE;
		}
		else if ((imageInfo.usage & VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT) == VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT)
		{
			binding.type = ShaderReflection::Type::COMBINED_IMAGE_SAMPLER;
		}
		else
		{
			Logger::Warning("Texture:" + GetFilepath().string() + " doesn't have any usage!");
		}

		m_UniformWriter = UniformWriter::Create(UniformLayout::Create(bindings), IsMultiBuffered());
		std::shared_ptr<VulkanUniformWriter> vkUniformWriter = std::dynamic_pointer_cast<VulkanUniformWriter>(m_UniformWriter);

		std::vector<VkDescriptorImageInfo> vkDescriptorImageInfos(m_ImageDatas.size());
		for (int i = 0; i < m_ImageDatas.size(); ++i)
		{
			vkDescriptorImageInfos[i] = GetDescriptorInfo(0, i);
		}

		// No need to flush, because it is a special function in VulkanUniformWriter, it flushes itself.
		vkUniformWriter->WriteTexture(0, vkDescriptorImageInfos);
	}
}

VulkanTexture::~VulkanTexture()
{
	for (auto& imageData : m_ImageDatas)
	{
		GetVkDevice()->DeleteResource([views = imageData.views]()
		{
			for (const auto& view : views)
			{
				vkDestroyImageView(GetVkDevice()->GetDevice(), view, nullptr);
			}
		});

		GetVkDevice()->DestroyImage(imageData.image, imageData.vmaAllocation, imageData.vmaAllocationInfo);
	}
}

VkDescriptorImageInfo VulkanTexture::GetDescriptorInfo(
	const uint32_t baseMipLevel,
	const uint32_t frameIndex)
{
	VkDescriptorImageInfo descriptorImageInfo{};
	descriptorImageInfo.imageLayout = GetLayout(frameIndex);
	descriptorImageInfo.imageView = GetImageView(baseMipLevel, frameIndex);
	descriptorImageInfo.sampler = m_Sampler;

	return descriptorImageInfo;
}

VkImageView VulkanTexture::CreateImageView(
	const VkImage image,
	const VkFormat format,
	const VkImageAspectFlagBits aspectMask,
	uint32_t baseMipLevel,
	const uint32_t mipLevels,
	uint32_t layerCount,
	VkImageViewType imageViewType)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = imageViewType;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layerCount;

	VkImageView imageView;
	if (vkCreateImageView(GetVkDevice()->GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to create texture image view!");
	}

	return imageView;
}

VkSampler VulkanTexture::CreateSampler(const Texture::SamplerCreateInfo& samplerCreateInfo)
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = ConvertFilter(samplerCreateInfo.filter);
	samplerInfo.minFilter = ConvertFilter(samplerCreateInfo.filter);
	samplerInfo.addressModeU = ConvertAddressMode(samplerCreateInfo.addressMode);
	samplerInfo.addressModeV = ConvertAddressMode(samplerCreateInfo.addressMode);
	samplerInfo.addressModeW = ConvertAddressMode(samplerCreateInfo.addressMode);
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = samplerCreateInfo.maxAnisotropy;
	samplerInfo.borderColor = ConvertBorderColor(samplerCreateInfo.borderColor);
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = ConvertMipmapMode(samplerCreateInfo.mipmapMode);
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

	return VulkanSamplerManager::GetInstance().CreateSampler(samplerInfo);
}

VkImageAspectFlagBits VulkanTexture::ConvertAspectMask(const AspectMask aspectMask)
{
	switch (aspectMask)
	{
	case Pengine::Texture::AspectMask::COLOR:
		return VK_IMAGE_ASPECT_COLOR_BIT;
	case Pengine::Texture::AspectMask::DEPTH:
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	FATAL_ERROR("Failed to convert aspect mask!");
	return VK_IMAGE_ASPECT_NONE;
}

Texture::AspectMask VulkanTexture::ConvertAspectMask(const VkImageAspectFlagBits aspectMask)
{
	switch (aspectMask)
	{
	case VK_IMAGE_ASPECT_COLOR_BIT:
		return Pengine::Texture::AspectMask::COLOR;
	case VK_IMAGE_ASPECT_DEPTH_BIT:
		return Pengine::Texture::AspectMask::DEPTH;
	}

	FATAL_ERROR("Failed to convert aspect mask!");
	return {};
}

VkImageLayout VulkanTexture::ConvertLayout(const Layout layout)
{
	return static_cast<VkImageLayout>(layout);
}

Texture::Layout VulkanTexture::ConvertLayout(const VkImageLayout layout)
{
	return static_cast<Layout>(layout);
}

VkImageUsageFlagBits VulkanTexture::ConvertUsage(Usage usage)
{
	switch (usage)
	{
	case Pengine::Texture::Usage::SAMPLED:
		return VK_IMAGE_USAGE_SAMPLED_BIT;
	case Pengine::Texture::Usage::TRANSFER_DST:
		return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	case Pengine::Texture::Usage::TRANSFER_SRC:
		return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	case Pengine::Texture::Usage::DEPTH_STENCIL_ATTACHMENT:
		return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	case Pengine::Texture::Usage::COLOR_ATTACHMENT:
		return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	case Pengine::Texture::Usage::STORAGE:
		return VK_IMAGE_USAGE_STORAGE_BIT;
	}

	FATAL_ERROR("Failed to convert usage!");
	return VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
}

Texture::Usage VulkanTexture::ConvertUsage(const VkImageUsageFlagBits usage)
{
	switch (usage)
	{
	case VK_IMAGE_USAGE_SAMPLED_BIT:
		return Pengine::Texture::Usage::SAMPLED;
	case VK_IMAGE_USAGE_TRANSFER_DST_BIT:
		return Pengine::Texture::Usage::TRANSFER_DST;
	case VK_IMAGE_USAGE_TRANSFER_SRC_BIT:
		return Pengine::Texture::Usage::TRANSFER_SRC;
	case VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
		return Pengine::Texture::Usage::DEPTH_STENCIL_ATTACHMENT;
	case VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
		return Pengine::Texture::Usage::COLOR_ATTACHMENT;
	case VK_IMAGE_USAGE_STORAGE_BIT:
		return Pengine::Texture::Usage::STORAGE;
	}

	FATAL_ERROR("Failed to convert usage!");
	return {};
}

VkSamplerAddressMode VulkanTexture::ConvertAddressMode(SamplerCreateInfo::AddressMode addressMode)
{
	switch (addressMode)
	{
	case Pengine::Texture::SamplerCreateInfo::AddressMode::REPEAT:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case Pengine::Texture::SamplerCreateInfo::AddressMode::MIRRORED_REPEAT:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case Pengine::Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_EDGE:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case Pengine::Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	case Pengine::Texture::SamplerCreateInfo::AddressMode::MIRROR_CLAMP_TO_EDGE:
		return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
	default:
		break;
	}

	FATAL_ERROR("Failed to convert address mode!");
	return {};
}

Texture::SamplerCreateInfo::AddressMode VulkanTexture::ConvertAddressMode(VkSamplerAddressMode addressMode)
{
	switch (addressMode)
	{
	case VK_SAMPLER_ADDRESS_MODE_REPEAT:
		return Pengine::Texture::SamplerCreateInfo::AddressMode::REPEAT;
	case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
		return Pengine::Texture::SamplerCreateInfo::AddressMode::MIRRORED_REPEAT;
	case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
		return Pengine::Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_EDGE;
	case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
		return Pengine::Texture::SamplerCreateInfo::AddressMode::CLAMP_TO_BORDER;
	case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
		return Pengine::Texture::SamplerCreateInfo::AddressMode::MIRROR_CLAMP_TO_EDGE;
	default:
		break;
	}

	FATAL_ERROR("Failed to convert address mode!");
	return {};
}

VkFilter VulkanTexture::ConvertFilter(SamplerCreateInfo::Filter filter)
{
	switch (filter)
	{
	case Pengine::Texture::SamplerCreateInfo::Filter::NEAREST:
		return VK_FILTER_NEAREST;
	case Pengine::Texture::SamplerCreateInfo::Filter::LINEAR:
		return VK_FILTER_LINEAR;
	default:
		break;
	}

	FATAL_ERROR("Failed to convert filter!");
	return {};
}

Texture::SamplerCreateInfo::Filter VulkanTexture::ConvertFilter(VkFilter filter)
{
	switch (filter)
	{
	case VK_FILTER_NEAREST:
		return Pengine::Texture::SamplerCreateInfo::Filter::NEAREST;
	case VK_FILTER_LINEAR:
		return Pengine::Texture::SamplerCreateInfo::Filter::LINEAR;
	default:
		break;
	}

	FATAL_ERROR("Failed to convert filter!");
	return {};
}

VkSamplerMipmapMode VulkanTexture::ConvertMipmapMode(Texture::SamplerCreateInfo::MipmapMode mipmapMode)
{
	switch (mipmapMode)
	{
	case Pengine::Texture::SamplerCreateInfo::MipmapMode::MODE_NEAREST:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	case Pengine::Texture::SamplerCreateInfo::MipmapMode::MODE_LINEAR:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	default:
		break;
	}

	FATAL_ERROR("Failed to convert mipmap mode!");
	return {};
}

Texture::SamplerCreateInfo::MipmapMode VulkanTexture::ConvertMipmapMode(VkSamplerMipmapMode mipmapMode)
{
	switch (mipmapMode)
	{
	case VK_SAMPLER_MIPMAP_MODE_NEAREST:
		return Pengine::Texture::SamplerCreateInfo::MipmapMode::MODE_NEAREST;
	case VK_SAMPLER_MIPMAP_MODE_LINEAR:
		return Pengine::Texture::SamplerCreateInfo::MipmapMode::MODE_LINEAR;
	default:
		break;
	}

	FATAL_ERROR("Failed to convert mipmap mode!");
	return {};
}

VkBorderColor VulkanTexture::ConvertBorderColor(Texture::SamplerCreateInfo::BorderColor borderColor)
{
	switch (borderColor)
	{
	case Pengine::Texture::SamplerCreateInfo::BorderColor::FLOAT_TRANSPARENT_BLACK:
		return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	case Pengine::Texture::SamplerCreateInfo::BorderColor::INT_TRANSPARENT_BLACK:
		return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	case Pengine::Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_BLACK:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	case Pengine::Texture::SamplerCreateInfo::BorderColor::INT_OPAQUE_BLACK:
		return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	case Pengine::Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_WHITE:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	case Pengine::Texture::SamplerCreateInfo::BorderColor::INT_OPAQUE_WHITE:
		return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	default:
		break;
	}

	FATAL_ERROR("Failed to convert border color!");
	return {};
}

Texture::SamplerCreateInfo::BorderColor VulkanTexture::ConvertBorderColor(VkBorderColor borderColor)
{
	switch (borderColor)
	{
	case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
		return Pengine::Texture::SamplerCreateInfo::BorderColor::FLOAT_TRANSPARENT_BLACK;
	case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
		return Pengine::Texture::SamplerCreateInfo::BorderColor::INT_TRANSPARENT_BLACK;
	case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
		return Pengine::Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_BLACK;
	case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
		return Pengine::Texture::SamplerCreateInfo::BorderColor::INT_OPAQUE_BLACK;
	case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
		return Pengine::Texture::SamplerCreateInfo::BorderColor::FLOAT_OPAQUE_WHITE;
	case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
		return Pengine::Texture::SamplerCreateInfo::BorderColor::INT_OPAQUE_WHITE;
	default:
		break;
	}

	FATAL_ERROR("Failed to convert border color!");
	return {};
}

void* VulkanTexture::GetId(const uint32_t frameIndex) const
{
	std::shared_ptr<VulkanUniformWriter> vkUniformWriter = std::dynamic_pointer_cast<VulkanUniformWriter>(m_UniformWriter);
	return (void*)vkUniformWriter->GetDescriptorSet(frameIndex);
}

void* VulkanTexture::GetData() const
{
	// TODO: Make possible get data from GPU.
	if (m_MemoryType != MemoryType::CPU)
	{
		FATAL_ERROR("Can't get data from the buffer that is allocated on GPU!");
	}

	return GetImageData().vmaAllocationInfo.pMappedData;
}

VulkanTexture::SubresourceLayout VulkanTexture::GetSubresourceLayout() const
{
	const ImageData& imageData = GetImageData();

	VkImageSubresource vkSubresource{};
	vkSubresource.aspectMask = ConvertAspectMask(m_AspectMask);
	VkSubresourceLayout vkSubresourceLayout;

	vkGetImageSubresourceLayout(GetVkDevice()->GetDevice(), imageData.image, &vkSubresource, &vkSubresourceLayout);

	SubresourceLayout subresourceLayout{};
	subresourceLayout.arrayPitch = vkSubresourceLayout.arrayPitch;
	subresourceLayout.depthPitch = vkSubresourceLayout.depthPitch;
	subresourceLayout.offset = vkSubresourceLayout.offset;
	subresourceLayout.rowPitch = vkSubresourceLayout.rowPitch;
	subresourceLayout.size = vkSubresourceLayout.size;

	return subresourceLayout;
}

void VulkanTexture::GenerateMipMaps(void* frame)
{
	VkCommandBuffer commandBuffer = GetVkDevice()->GetCommandBufferFromFrame(frame);

	GetVkDevice()->GenerateMipMaps(
		GetImageData().image,
		ConvertFormat(m_Format),
		GetSize().x,
		GetSize().y,
		GetMipLevels(),
		GetLayerCount(),
		commandBuffer);
}

void VulkanTexture::Copy(
	std::shared_ptr<Texture> src,
	const Region& region,
	void* frame)
{
	VkCommandBuffer commandBuffer = GetVkDevice()->GetCommandBufferFromFrame(frame);

	std::shared_ptr<VulkanTexture> vkSrc = std::static_pointer_cast<VulkanTexture>(src);

	ImageData& srcImageData = vkSrc->GetImageData();
	ImageData& dstImageData = GetImageData();

	Vk::GetVkDevice()->CopyImageToImage(
		srcImageData.image,
		srcImageData.m_Layout,
		dstImageData.image,
		dstImageData.m_Layout,
		region.srcOffset,
		region.dstOffset,
		region.extent,
		commandBuffer);
}

void VulkanTexture::Transition(Layout layout, void* frame)
{
	VkCommandBuffer commandBuffer = GetVkDevice()->GetCommandBufferFromFrame(frame);

	TransitionInternal(GetImageData(), ConvertLayout(layout), commandBuffer);
}

void VulkanTexture::TransitionInternal(ImageData& imageData, VkImageLayout layout, VkCommandBuffer commandBuffer)
{
	imageData.m_PreviousLayout = imageData.m_Layout;
	imageData.m_Layout = layout;

	GetVkDevice()->TransitionImageLayout(
		imageData.image,
		ConvertFormat(m_Format),
		ConvertAspectMask(m_AspectMask),
		m_MipLevels,
		m_LayerCount,
		imageData.m_PreviousLayout,
		imageData.m_Layout,
		commandBuffer);
}

VulkanTexture::ImageData& VulkanTexture::GetImageData()
{
	return m_IsMultiBuffered ? m_ImageDatas[frameInFlightIndex] : m_ImageDatas[0];
}

const VulkanTexture::ImageData& VulkanTexture::GetImageData() const
{
	return m_IsMultiBuffered ? m_ImageDatas[frameInFlightIndex] : m_ImageDatas[0];
}
