#include "VulkanHeadlessWindow.h"

#include "../Core/Logger.h"

#include "VulkanDevice.h"

using namespace Pengine;
using namespace Vk;

VulkanHeadlessWindow::VulkanHeadlessWindow(const std::string& title, const std::string& name, const glm::ivec2& size)
	: Window(title, name, size)
{
	m_Frame.CommandPool = GetVkDevice()->CreateCommandPool();
	m_Frame.CommandBuffer = GetVkDevice()->CreateCommandBuffer(m_Frame.CommandPool);
	m_Frame.Fence = GetVkDevice()->CreateFence();

	m_IsHeadless = true;
}

VulkanHeadlessWindow::~VulkanHeadlessWindow()
{
	GetVkDevice()->DeleteResource([frame = m_Frame]()
	{
		vkFreeCommandBuffers(GetVkDevice()->GetDevice(), frame.CommandPool, 1, &frame.CommandBuffer);
		vkDestroyCommandPool(GetVkDevice()->GetDevice(), frame.CommandPool, nullptr);
		vkDestroyFence(GetVkDevice()->GetDevice(), frame.Fence, nullptr);
	});
}

void VulkanHeadlessWindow::Update()
{
}

bool VulkanHeadlessWindow::Resize(const glm::ivec2& size)
{
	return Window::Resize(size);
}

void VulkanHeadlessWindow::NewFrame()
{
	Logger::Error("Headless window can't call NewFrame!");
}

void VulkanHeadlessWindow::EndFrame()
{
	Logger::Error("Headless window can't call EndFrame!");
}

void VulkanHeadlessWindow::Clear(const glm::vec4& color)
{
	Logger::Error("Headless window can't call Clear!");
}

void VulkanHeadlessWindow::Present(std::shared_ptr<Texture> texture)
{
	Logger::Error("Headless window can't call Present!");
}

void VulkanHeadlessWindow::ImGuiBegin()
{
	Logger::Error("Headless window can't call ImGuiBegin!");
}

void VulkanHeadlessWindow::ImGuiEnd()
{
	Logger::Error("Headless window can't call ImGuiEnd!");
}

void* VulkanHeadlessWindow::BeginFrame()
{
	VulkanFrameInfo* vkFrame = &m_Frame;

	if (vkWaitForFences(GetVkDevice()->GetDevice(), 1, &vkFrame->Fence, VK_TRUE, UINT64_MAX))
	{
		FATAL_ERROR("Failed to wait for fences!");
		return nullptr;
	}

	if (vkResetFences(GetVkDevice()->GetDevice(), 1, &vkFrame->Fence))
	{
		FATAL_ERROR("Failed to reset fences!");
		return nullptr;
	}

	if (vkResetCommandPool(GetVkDevice()->GetDevice(), vkFrame->CommandPool, 0))
	{
		FATAL_ERROR("Failed to reset command pool!");
		return nullptr;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(vkFrame->CommandBuffer, &beginInfo) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to begin recording command buffer!");
		return nullptr;
	}

	return vkFrame;
}

void VulkanHeadlessWindow::EndFrame(void* frame)
{
	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	const VkCommandBuffer commandBuffer = vkFrame->CommandBuffer;

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to record command buffer!");
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkFrame->CommandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	VulkanDevice::Lock lock;

	if (vkQueueSubmit(GetVkDevice()->GetGraphicsQueue(), 1, &submitInfo,
		vkFrame->Fence) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to submit draw command buffer!");
	}
}

void VulkanHeadlessWindow::ImGuiRenderPass()
{
	//Logger::Error("Headless window can't call ImGuiRenderPass!");
}

void VulkanHeadlessWindow::DisableCursor()
{
	Logger::Error("Headless window can't call DisableCursor!");
}

void VulkanHeadlessWindow::ShowCursor()
{
	Logger::Error("Headless window can't call ShowCursor!");
}

void VulkanHeadlessWindow::HideCursor()
{
	Logger::Error("Headless window can't call HideCursor!");
}

void VulkanHeadlessWindow::SetTitle(const std::string& title)
{
	m_Title = title;
}
