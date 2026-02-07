#pragma once

#include "../Core/Core.h"
#include "../Graphics/FrameBuffer.h"
#include "../Vulkan/VulkanFrameBuffer.h"

#include <vulkan/vulkan.h>

namespace Pengine::Vk
{

	class PENGINE_API VulkanFrameBuffer final : public FrameBuffer
	{
	public:
		VulkanFrameBuffer(const std::vector<Texture::CreateInfo>& attachments,
			std::shared_ptr<RenderPass> renderPass,
			RenderView* renderView);
		virtual ~VulkanFrameBuffer() override;
		VulkanFrameBuffer(const VulkanFrameBuffer&) = delete;
		VulkanFrameBuffer& operator=(const VulkanFrameBuffer&) = delete;

		[[nodiscard]] VkFramebuffer GetFrameBuffer() const { return m_FrameBuffers[frameInFlightIndex]; }

		[[nodiscard]] virtual std::shared_ptr<Texture> GetAttachment(const size_t index) const override
		{ return m_Attachments[index]; }

		[[nodiscard]] virtual std::vector<std::shared_ptr<Texture>> GetAttachments() const override
		{ return m_Attachments; }

		virtual void Resize(const glm::ivec2& size) override;

		virtual void Clear() override;

	private:
		std::vector<VkFramebuffer> m_FrameBuffers;
		std::vector<std::shared_ptr<Texture>> m_Attachments;
	};

}
