#include "VulkanFrameBuffer.h"

#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "VulkanRenderPass.h"

#include "../Core/Logger.h"

using namespace Pengine;
using namespace Vk;

VulkanFrameBuffer::VulkanFrameBuffer(
	const std::vector<Texture::CreateInfo>& attachments,
	std::shared_ptr<RenderPass> renderPass,
	RenderView* renderView)
	: FrameBuffer(attachments, renderPass, renderView)
{
	if (m_AttachmentCreateInfos.empty())
	{
		FATAL_ERROR("Frame buffer attachments are empty!");
	}

	Resize(!m_AttachmentCreateInfos.empty() ? m_AttachmentCreateInfos[0].size : glm::ivec2{ 0, 0 });
}

VulkanFrameBuffer::~VulkanFrameBuffer()
{
	Clear();
}

void VulkanFrameBuffer::Resize(const glm::ivec2& size)
{
	Clear();

	m_Size = size;

	m_FrameBuffers.resize(frameInFlightCount);

	const auto& renderPassAttachments = m_RenderPass->GetAttachmentDescriptions();

	uint32_t layers = 1;

	size_t textureIndex = 0;
	for (Texture::CreateInfo& textureCreateInfo : m_AttachmentCreateInfos)
	{
		std::shared_ptr<Texture> texture;
		if (renderPassAttachments[textureIndex].getFrameBufferCallback && m_RenderView)
		{
			texture = renderPassAttachments[textureIndex].getFrameBufferCallback(m_RenderView);
		}
		else
		{
			textureCreateInfo.size = m_Size;
			texture = Texture::Create(textureCreateInfo);
		}

		m_Attachments.emplace_back(texture);

		layers = std::max(layers, texture->GetLayerCount());
		textureIndex++;
	}

	for (size_t frameIndex = 0; frameIndex < frameInFlightCount; frameIndex++)
	{
		std::vector<VkImageView> imageViews;

		for (int i = 0; i < textureIndex; ++i)
		{
			std::shared_ptr<VulkanTexture> vkTexture = std::dynamic_pointer_cast<VulkanTexture>(m_Attachments[i]);
			imageViews.emplace_back(vkTexture->GetImageView(frameIndex));
		}

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = std::static_pointer_cast<VulkanRenderPass>(m_RenderPass)->GetRenderPass();
		framebufferInfo.attachmentCount = static_cast<uint32_t>(imageViews.size());
		framebufferInfo.pAttachments = imageViews.data();
		framebufferInfo.width = static_cast<uint32_t>(m_Size.x);
		framebufferInfo.height = static_cast<uint32_t>(m_Size.y);
		framebufferInfo.layers = layers;

		if (vkCreateFramebuffer(
			GetVkDevice()->GetDevice(),
			&framebufferInfo,
			nullptr,
			&m_FrameBuffers[frameIndex]) != VK_SUCCESS)
		{
			FATAL_ERROR("Failed to create framebuffer!");
		}
	}
}

void VulkanFrameBuffer::Clear()
{
	for (const VkFramebuffer frameBuffer : m_FrameBuffers)
	{
		GetVkDevice()->DeleteResource([frameBuffer]()
		{
			vkDestroyFramebuffer(GetVkDevice()->GetDevice(), frameBuffer, nullptr);
		});
	}
	
	m_FrameBuffers.clear();
	m_Attachments.clear();
}
