#include "RenderPass.h"

#include "../Core/Logger.h"
#include "../Utils/Utils.h"
#include "../Vulkan/VulkanRenderPass.h"

using namespace Pengine;

std::shared_ptr<RenderPass> RenderPass::Create(const CreateInfo& createInfo)
{
	if (graphicsAPI == GraphicsAPI::Vk)
	{
		return std::make_shared<Vk::VulkanRenderPass>(createInfo);
	}

	FATAL_ERROR("Failed to create the renderer, no graphics API implementation");
	return nullptr;
}

RenderPass::RenderPass(const CreateInfo& createInfo)
	: Pass(createInfo.type, createInfo.name, createInfo.executeCallback, createInfo.createCallback)
	, m_AttachmentDescriptions(createInfo.attachmentDescriptions)
	, m_ClearColors(createInfo.clearColors)
	, m_ClearDepths(createInfo.clearDepths)
	, m_ResizeViewportScale(createInfo.resizeViewportScale)
	, m_ResizeWithViewport(createInfo.resizeWithViewport)
	, m_CreateFrameBuffer(createInfo.createFrameBuffer)
	, m_IsFrameBufferMultiBuffered(createInfo.isFrameBufferMultiBuffered)
{
}

void RenderPass::Execute(const RenderCallbackInfo& renderInfo) const
{
	if (m_ExecuteCallback)
	{
		m_ExecuteCallback(renderInfo);
	}
}
