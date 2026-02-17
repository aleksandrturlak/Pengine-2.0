#pragma once

#include "../Core/Core.h"
#include "../Graphics/UniformWriter.h"

#include <vulkan/vulkan.h>

#include <set>

namespace Pengine::Vk
{

	class PENGINE_API VulkanUniformWriter final : public UniformWriter
	{
	public:
		explicit VulkanUniformWriter(
			std::shared_ptr<UniformLayout> uniformLayout,
			bool isMultiBuffered);
		virtual ~VulkanUniformWriter() override;
		VulkanUniformWriter(const VulkanUniformWriter&) = delete;
		VulkanUniformWriter& operator=(const VulkanUniformWriter&) = delete;

		virtual void Flush() override;

		virtual NativeHandle GetNativeHandle() const override;

		void WriteTexture(uint32_t location, const std::vector<VkDescriptorImageInfo>& vkDescriptorImageInfos);

		[[nodiscard]] VkDescriptorSet GetDescriptorSet(const uint32_t frameIndex = Vk::frameInFlightIndex) const;

	private:
		std::vector<VkDescriptorSet> m_DescriptorSets;
	};

}
