#pragma once

#include "../Core/Core.h"
#include "../Graphics/Texture.h"

#include <vma/vk_mem_alloc.h>

namespace Pengine::Vk
{

	class PENGINE_API VulkanTexture final : public Texture
	{
	public:
		explicit VulkanTexture(const CreateInfo& createInfo);
		virtual ~VulkanTexture() override;
		VulkanTexture(const VulkanTexture&) = delete;
		VulkanTexture(VulkanTexture&&) = delete;
		VulkanTexture& operator=(const VulkanTexture&) = delete;
		VulkanTexture& operator=(VulkanTexture&&) = delete;

		VkDescriptorImageInfo GetDescriptorInfo(const uint32_t baseMipLevel = 0, const uint32_t frameIndex = Vk::frameInFlightIndex);

		static VkImageView CreateImageView(
			VkImage image,
			VkFormat format,
			VkImageAspectFlagBits aspectMask,
			uint32_t baseMipLevel,
			uint32_t mipLevels,
			uint32_t layerCount,
			VkImageViewType imageViewType);

		static VkSampler CreateSampler(const Texture::SamplerCreateInfo& samplerCreateInfo);

		static VkImageAspectFlagBits ConvertAspectMask(AspectMask aspectMask);

		static AspectMask ConvertAspectMask(VkImageAspectFlagBits aspectMask);

		static VkImageLayout ConvertLayout(Layout layout);

		static Layout ConvertLayout(VkImageLayout layout);

		static VkImageUsageFlagBits ConvertUsage(Usage usage);

		static Usage ConvertUsage(VkImageUsageFlagBits usage);

		static VkSamplerAddressMode ConvertAddressMode(SamplerCreateInfo::AddressMode addressMode);

		static SamplerCreateInfo::AddressMode ConvertAddressMode(VkSamplerAddressMode addressMode);

		static VkFilter ConvertFilter(SamplerCreateInfo::Filter filter);

		static SamplerCreateInfo::Filter ConvertFilter(VkFilter filter);

		static VkSamplerMipmapMode ConvertMipmapMode(SamplerCreateInfo::MipmapMode mipmapMode);

		static SamplerCreateInfo::MipmapMode ConvertMipmapMode(VkSamplerMipmapMode mipmapMode);

		static VkBorderColor ConvertBorderColor(SamplerCreateInfo::BorderColor borderColor);

		static SamplerCreateInfo::BorderColor ConvertBorderColor(VkBorderColor borderColor);

		[[nodiscard]] virtual void* GetId(const uint32_t frameIndex = Vk::frameInFlightIndex) const override;

		[[nodiscard]] virtual void* GetData() const override;

		[[nodiscard]] virtual SubresourceLayout GetSubresourceLayout() const override;

		virtual void GenerateMipMaps(void* frame = nullptr) override;

		virtual void Copy(std::shared_ptr<Texture> src, const Region& region, void* frame = nullptr) override;

		virtual void Transition(Layout layout, void* frame = nullptr) override;

		[[nodiscard]] VkImageView GetImageView(
			const uint32_t baseMipLevel = 0,
			const uint32_t frameIndex = Vk::frameInFlightIndex) const
			{ return m_IsMultiBuffered ? m_ImageDatas[frameIndex].views[baseMipLevel] : m_ImageDatas[0].views[baseMipLevel]; }

		[[nodiscard]] VkImage GetImage(const uint32_t frameIndex = Vk::frameInFlightIndex) const { return m_IsMultiBuffered ? m_ImageDatas[frameIndex].image : m_ImageDatas[0].image; }

		[[nodiscard]] VkSampler GetSampler() const { return m_Sampler; }

		[[nodiscard]] VkImageLayout GetLayout(const uint32_t frameIndex = Vk::frameInFlightIndex) const { return m_IsMultiBuffered ? m_ImageDatas[frameIndex].m_Layout : m_ImageDatas[0].m_Layout; }

	private:
		struct ImageData
		{
			VkImage image{};
			VmaAllocation vmaAllocation = VK_NULL_HANDLE;
			VmaAllocationInfo vmaAllocationInfo{};
			VkImageLayout m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageLayout m_PreviousLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			std::vector<VkImageView> views;
		};

		void TransitionInternal(ImageData& imageData, VkImageLayout layout, VkCommandBuffer commandBuffer = VK_NULL_HANDLE);

		ImageData& GetImageData();

		const ImageData& GetImageData() const;

		std::vector<ImageData> m_ImageDatas;
		VkSampler m_Sampler{};
	};

}