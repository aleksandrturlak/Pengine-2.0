#include "FrameBuffer.h"

#include "../Core/Logger.h"
#include "../Vulkan/VulkanFrameBuffer.h"

using namespace Pengine;

std::shared_ptr<FrameBuffer> FrameBuffer::Create(
	std::shared_ptr<RenderPass> renderPass,
	RenderView* renderView,
	const glm::ivec2& size)
{
	if (!renderPass)
	{
		FATAL_ERROR("Render pass is nullptr!");
		return nullptr;
	}

	std::vector<Texture::CreateInfo> attachments;
	for (const auto& attachment : renderPass->GetAttachmentDescriptions())
	{
		Texture::CreateInfo& attachmentCreateInfo = attachments.emplace_back(attachment.textureCreateInfo);
		attachmentCreateInfo.size = attachmentCreateInfo.size.x == 0 &&
			attachmentCreateInfo.size.y == 0 ? glm::ivec2((glm::vec2)size * renderPass->GetResizeViewportScale()) : attachmentCreateInfo.size;
		attachmentCreateInfo.isMultiBuffered = renderPass->IsFrameBufferMultiBuffered();
	}

	if (graphicsAPI == GraphicsAPI::Vk)
	{
		return std::make_shared<Vk::VulkanFrameBuffer>(attachments, renderPass, renderView);
	}

	FATAL_ERROR("Failed to create the framebuffer, no graphics API implementation");
	return nullptr;
}

FrameBuffer::FrameBuffer(
	const std::vector<Texture::CreateInfo>& attachments,
	std::shared_ptr<RenderPass> renderPass,
	RenderView* renderView)
	: m_RenderPass(std::move(renderPass))
	, m_AttachmentCreateInfos(attachments)
	, m_RenderView(renderView)
{
	if (attachments.empty())
	{
		FATAL_ERROR("Render pass attachments must contain at least one attachment!");
	}
}
