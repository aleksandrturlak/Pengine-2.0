#include "VulkanWindow.h"

#include "../Core/Input.h"
#include "../Core/Logger.h"
#include "../Core/WindowManager.h"
#include "../Core/Profiler.h"
#include "../Utils/Utils.h"
#include "../Vulkan/VulkanDevice.h"
#include "../Vulkan/VulkanDescriptors.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>

#include <locale>

using namespace Pengine;
using namespace Vk;

VulkanWindow::VulkanWindow(const std::string& title, const std::string& name, const glm::ivec2& size)
	: Window(title, name, size)
{
	PROFILER_SCOPE(__FUNCTION__);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_Window = glfwCreateWindow(size.x, size.y, title.c_str(), NULL, NULL);

	if (!m_Window)
	{
		glfwTerminate();

		FATAL_ERROR("Failed to create window!");
	}

	glfwMakeContextCurrent(m_Window);

	m_Surface = GetVkDevice()->CreateSurface(m_Window);

	glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* glfwWindow, int width, int height)
	{
		if (auto window = WindowManager::GetInstance().GetWindowByGLFW(glfwWindow))
		{
			window->SetContextCurrent();
			window->Resize({ width, height });
		}
	});

	InitializeImGui();

	Resize(size);

	auto keyCallback = [](GLFWwindow* glfwWindow, int key, int scancode, int action, int mods)
	{
		auto window = WindowManager::GetInstance().GetWindowByGLFW(glfwWindow);
		if (!window)
		{
			return;
		}
		
		window->SetContextCurrent();
		Input::GetInstance(window.get()).KeyCallback(key, scancode, action, mods);
		ImGui_ImplGlfw_KeyCallback(glfwWindow, key, scancode, action, mods);
	};
	glfwSetKeyCallback(m_Window, keyCallback);

	auto mouseButtonCallback = [](GLFWwindow* glfwWindow, int button, int action, int mods)
	{
		auto window = WindowManager::GetInstance().GetWindowByGLFW(glfwWindow);
		if (!window)
		{
			return;
		}

		window->SetContextCurrent();
		Input::GetInstance(window.get()).MouseButtonCallback(button, action, mods);
		ImGui_ImplGlfw_MouseButtonCallback(glfwWindow, button, action, mods);
	};
	glfwSetMouseButtonCallback(m_Window, mouseButtonCallback);

	auto cursorPositionCallback = [](GLFWwindow* glfwWindow, double x, double y)
	{
		auto window = WindowManager::GetInstance().GetWindowByGLFW(glfwWindow);
		if (!window)
		{
			return;
		}

		window->SetContextCurrent();
		Input::GetInstance(window.get()).MousePositionCallback(x, y);
		ImGui_ImplGlfw_CursorPosCallback(glfwWindow, x, y);
	};
	glfwSetCursorPosCallback(m_Window, cursorPositionCallback);

	auto isKeyDownCallback = [this](int keycode)
	{
		auto window = WindowManager::GetInstance().GetWindowByGLFW(m_Window);
		if (!window)
		{
			return GLFW_RELEASE;
		}

		window->SetContextCurrent();
		return glfwGetKey(m_Window, keycode);
	};
	Input::GetInstance(this).SetIsKeyDownCallback(isKeyDownCallback);

	auto isMouseDownCallback = [this](int keycode)
	{
		auto window = WindowManager::GetInstance().GetWindowByGLFW(m_Window);
		if (!window)
		{
			return GLFW_RELEASE;
		}

		window->SetContextCurrent();
		return glfwGetMouseButton(m_Window, keycode);
	};
	Input::GetInstance(this).SetIsMouseDownCallback(isMouseDownCallback);
}

VulkanWindow::~VulkanWindow()
{
	PROFILER_SCOPE(__FUNCTION__);

	GetVkDevice()->WaitIdle();

	Input::RemoveInstance(this);

	SetContextCurrent();
	glfwPollEvents();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	ImGui::DestroyContext();

	ImGui_ImplVulkanH_DestroyWindow(GetVkDevice()->GetInstance(), GetVkDevice()->GetDevice(), &m_VulkanWindow, nullptr);
	vkDestroyDescriptorPool(GetVkDevice()->GetDevice(), m_ImGuiDescriptorPool, nullptr);
	glfwDestroyWindow(m_Window);
}

void VulkanWindow::Update()
{
}

bool VulkanWindow::Resize(const glm::ivec2& size)
{
	PROFILER_SCOPE(__FUNCTION__);

	if (!Window::Resize(size))
	{
		return false;
	}

	SwapChainSupportDetails swapChainSupport = GetVkDevice()->GetSwapChainSupport(m_Surface);
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 &&
		imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	ImGui_ImplVulkanH_CreateOrResizeWindow(
		GetVkDevice()->GetInstance(),
		GetVkDevice()->GetPhysicalDevice(),
		GetVkDevice()->GetDevice(),
		&m_VulkanWindow,
		GetVkDevice()->GetGraphicsFamilyIndex(),
		nullptr,
		m_Size.x,
		m_Size.y,
		imageCount);
	ImGui_ImplVulkan_SetMinImageCount(imageCount);

	frameInFlightCount = m_VulkanWindow.FrameInFlightCount;
	swapChainImageCount = m_VulkanWindow.ImageCount;
	m_Frames.resize(frameInFlightCount);

	for (size_t i = 0; i < frameInFlightCount; i++)
	{
		m_Frames[i].CommandBuffer = m_VulkanWindow.Frames[i].CommandBuffer;
		m_Frames[i].CommandPool = m_VulkanWindow.Frames[i].CommandPool;
		m_Frames[i].Fence = m_VulkanWindow.Frames[i].Fence;
	}

	return true;
}

void VulkanWindow::NewFrame()
{
	PROFILER_SCOPE(__FUNCTION__);

	Input::GetInstance(this).ResetInput();

	glfwMakeContextCurrent(m_Window);

	glfwPollEvents();
	SetIsRunning(!glfwWindowShouldClose(m_Window));
}

void VulkanWindow::EndFrame()
{
	PROFILER_SCOPE(__FUNCTION__);
}

void VulkanWindow::Clear(const glm::vec4& color)
{
}

void VulkanWindow::Present(std::shared_ptr<Texture> texture)
{
}

void VulkanWindow::ImGuiBegin()
{
	PROFILER_SCOPE(__FUNCTION__);

	ImGui::SetCurrentContext(m_ImGuiContext);

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	static bool dockspaceOpen = true;
	static bool optFullscreenPersistant = true;
	bool optFullscreen = optFullscreenPersistant;
	static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

	if (optFullscreen)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}

	if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
	{
		windowFlags |= ImGuiWindowFlags_NoBackground;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", &dockspaceOpen, windowFlags);
	ImGui::PopStyleVar();

	if (optFullscreen)
	{
		ImGui::PopStyleVar(2);
	}

	const ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	const float minWinSizeX = style.WindowMinSize.x;
	style.WindowMinSize.x = 370.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspaceFlags);
	}

	style.WindowMinSize.x = minWinSizeX;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
}

void VulkanWindow::ImGuiEnd()
{
	PROFILER_SCOPE(__FUNCTION__);

	ImGui::PopStyleVar();
	ImGui::End();
}

void* VulkanWindow::BeginFrame()
{
	PROFILER_SCOPE(__FUNCTION__);

	VulkanFrameInfo* vkFrame = &(m_Frames[frameInFlightIndex]);

	VkResult result = vkWaitForFences(GetVkDevice()->GetDevice(), 1, &vkFrame->Fence, VK_TRUE, UINT64_MAX);
	if (result != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to wait for fences!");
		return nullptr;
	}

	if (vkResetFences(GetVkDevice()->GetDevice(), 1, &vkFrame->Fence) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to reset fences!");
		return nullptr;
	}

	const VkSemaphore imageAcquiredSemaphore = m_VulkanWindow.FrameSemaphores[m_VulkanWindow.SemaphoreIndex].ImageAcquiredSemaphore;
	result = vkAcquireNextImageKHR(
		GetVkDevice()->GetDevice(),
		m_VulkanWindow.Swapchain,
		(std::numeric_limits<uint64_t>::max)(),
		imageAcquiredSemaphore,
		VK_NULL_HANDLE,
		&m_VulkanWindow.ImageIndex);

	swapChainImageIndex = m_VulkanWindow.ImageIndex;

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		FATAL_ERROR("Failed to recreate swap chain image!");
		return nullptr;
	}

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		FATAL_ERROR("Failed to acquire swap chain image!");
		return nullptr;
	}

	if (vkResetCommandPool(GetVkDevice()->GetDevice(), vkFrame->CommandPool, 0) != VK_SUCCESS)
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

void VulkanWindow::EndFrame(void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	const VkCommandBuffer commandBuffer = vkFrame->CommandBuffer;

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to record command buffer!");
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_VulkanWindow.FrameSemaphores[m_VulkanWindow.SemaphoreIndex].ImageAcquiredSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkFrame->CommandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_VulkanWindow.FrameSemaphores[m_VulkanWindow.SemaphoreIndex].RenderCompleteSemaphore;

	VulkanDevice::Lock lock;

	VkResult result = vkQueueSubmit(GetVkDevice()->GetGraphicsQueue(), 1, &submitInfo, vkFrame->Fence);
	if (result != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_VulkanWindow.FrameSemaphores[m_VulkanWindow.SemaphoreIndex].RenderCompleteSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_VulkanWindow.Swapchain;
	presentInfo.pImageIndices = &m_VulkanWindow.ImageIndex;

	result = vkQueuePresentKHR(GetVkDevice()->GetGraphicsQueue(), &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		Resize(GetSize());
	}
	else if (result != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to present swap chain image!");
	}

	m_VulkanWindow.SemaphoreIndex = (m_VulkanWindow.SemaphoreIndex + 1) % m_VulkanWindow.SemaphoreCount;
	frameInFlightIndex = m_VulkanWindow.FrameIndex = (m_VulkanWindow.FrameIndex + 1) % m_VulkanWindow.FrameInFlightCount;
}

void VulkanWindow::ImGuiRenderPass()
{
	PROFILER_SCOPE(__FUNCTION__);

	ImGui::SetCurrentContext(m_ImGuiContext);

	const ImGui_ImplVulkanH_Frame& imGuiFrame = m_VulkanWindow.Frames[m_VulkanWindow.FrameIndex];

	GetVkDevice()->CommandBeginLabel("ImGui", imGuiFrame.CommandBuffer, topLevelRenderPassDebugColor);

	VkRenderPassBeginInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = m_VulkanWindow.RenderPass;
	info.framebuffer = m_VulkanWindow.Framebuffers[m_VulkanWindow.ImageIndex];
	info.renderArea.extent.width = m_VulkanWindow.Width;
	info.renderArea.extent.height = m_VulkanWindow.Height;
	info.clearValueCount = 1;
	info.pClearValues = &m_VulkanWindow.ClearValue;
	vkCmdBeginRenderPass(imGuiFrame.CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = static_cast<float>(m_VulkanWindow.Width);
	viewport.height = static_cast<float>(m_VulkanWindow.Height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{ { 0, 0}, { (uint32_t)m_VulkanWindow.Width, (uint32_t)m_VulkanWindow.Height } };
	vkCmdSetViewport(imGuiFrame.CommandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(imGuiFrame.CommandBuffer, 0, 1, &scissor);

	ImGui::Render();

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData)
	{
		ImGui_ImplVulkan_RenderDrawData(drawData, imGuiFrame.CommandBuffer);
	}

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	vkCmdEndRenderPass(imGuiFrame.CommandBuffer);

	GetVkDevice()->CommandEndLabel(imGuiFrame.CommandBuffer);
}

void VulkanWindow::DisableCursor()
{
	glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void VulkanWindow::ShowCursor()
{
	glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void VulkanWindow::HideCursor()
{
	glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

void VulkanWindow::SetTitle(const std::string& title)
{
	m_Title = title;
	glfwSetWindowTitle(m_Window, m_Title.c_str());
}

VkExtent2D VulkanWindow::GetExtent() const
{
	return { static_cast<uint32_t>(m_Size.x), static_cast<uint32_t>(m_Size.y) };
}

void VulkanWindow::InitializeImGui()
{
	// Create Descriptor Pool
	{
		const std::vector<VkDescriptorPoolSize> poolSizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		poolInfo.maxSets = 1000 * poolSizes.size();
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		if (vkCreateDescriptorPool(GetVkDevice()->GetDevice(), &poolInfo, nullptr, &m_ImGuiDescriptorPool) !=
			VK_SUCCESS)
		{
			FATAL_ERROR("Failed to create descriptor pool for ImGui!");
		}
	}

	// Create Framebuffers
	m_VulkanWindow.Surface = m_Surface;
	constexpr VkFormat requestSurfaceImageFormat[] =
	{ 
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_R8G8B8_UNORM
	};
	constexpr VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	m_VulkanWindow.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
		GetVkDevice()->GetPhysicalDevice(),
		m_VulkanWindow.Surface, requestSurfaceImageFormat,
		IM_ARRAYSIZE(requestSurfaceImageFormat),
		requestSurfaceColorSpace);

#define UNLIMITED_FRAME_RATE
#ifdef UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif

	m_VulkanWindow.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(GetVkDevice()->GetPhysicalDevice(), m_VulkanWindow.Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
	
	SwapChainSupportDetails swapChainSupport = GetVkDevice()->GetSwapChainSupport(m_Surface);
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 &&
		imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	ImGui_ImplVulkanH_CreateOrResizeWindow(
		GetVkDevice()->GetInstance(),
		GetVkDevice()->GetPhysicalDevice(),
		GetVkDevice()->GetDevice(),
		&m_VulkanWindow,
		GetVkDevice()->GetGraphicsFamilyIndex(),
		nullptr,
		m_Size.x,
		m_Size.y,
		imageCount);

	frameInFlightCount = m_VulkanWindow.FrameInFlightCount;
	swapChainImageCount = m_VulkanWindow.ImageCount;
	m_Frames.resize(frameInFlightCount);

	m_ImGuiContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(m_ImGuiContext);
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("Editor/Fonts/Calibri.ttf", 16);
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(m_Window, true);
	ImGui_ImplVulkan_InitInfo initializeInfo{};
	initializeInfo.Instance = GetVkDevice()->GetInstance();
	initializeInfo.PhysicalDevice = GetVkDevice()->GetPhysicalDevice();
	initializeInfo.Device = GetVkDevice()->GetDevice();
	initializeInfo.QueueFamily = GetVkDevice()->GetGraphicsFamilyIndex();
	initializeInfo.Queue = GetVkDevice()->GetGraphicsQueue();
	initializeInfo.DescriptorPool = m_ImGuiDescriptorPool;
	initializeInfo.MinImageCount = imageCount;
	initializeInfo.ImageCount = m_VulkanWindow.ImageCount;
	initializeInfo.FrameInFlightCount = m_VulkanWindow.FrameInFlightCount;
	initializeInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initializeInfo.UseDynamicRendering = false;
	initializeInfo.RenderPass = m_VulkanWindow.RenderPass;

	ImGui_ImplVulkan_Init(&initializeInfo);
	ImGui_ImplVulkan_SetMinImageCount(imageCount);

	for (size_t i = 0; i < frameInFlightCount; i++)
	{
		m_Frames[i].CommandBuffer = m_VulkanWindow.Frames[i].CommandBuffer;
		m_Frames[i].CommandPool = m_VulkanWindow.Frames[i].CommandPool;
		m_Frames[i].Fence = m_VulkanWindow.Frames[i].Fence;
	}

	// Upload Fonts
	{
		VulkanDevice::Lock lock;

		// Use any command queue
		const VkCommandPool commandPool = m_VulkanWindow.Frames[m_VulkanWindow.FrameIndex].CommandPool;
		const VkCommandBuffer commandBuffer = m_VulkanWindow.Frames[m_VulkanWindow.FrameIndex].CommandBuffer;

		if (vkResetCommandPool(GetVkDevice()->GetDevice(), commandPool, 0) != VK_SUCCESS)
		{
			FATAL_ERROR("Failed to reset command pool!");
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			FATAL_ERROR("Failed to begin command pool!");
		}

		ImGui_ImplVulkan_CreateFontsTexture();

		VkSubmitInfo endInfo{};
		endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		endInfo.commandBufferCount = 1;
		endInfo.pCommandBuffers = &commandBuffer;
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			FATAL_ERROR("Failed to end command pool!");
		}

		if (vkQueueSubmit(GetVkDevice()->GetGraphicsQueue(), 1, &endInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			FATAL_ERROR("Failed to submit queue!");
		}

		ImGui_ImplVulkan_DestroyFontsTexture();
	}
}
