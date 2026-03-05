#pragma once

#include "../Core/Core.h"

#include "../Graphics/Device.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vma/vk_mem_alloc.h>

#include "VulkanDescriptors.h"

#include <deque>
#include <mutex>
#include <vector>

namespace Pengine::Vk
{

	struct PENGINE_API SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct PENGINE_API QueueFamilyIndices
	{
		uint32_t graphicsFamily = 0;
		bool graphicsFamilyHasValue = false;

		[[nodiscard]] bool IsComplete() const { return graphicsFamilyHasValue; }
	};

	[[nodiscard]] std::shared_ptr<class VulkanDevice> GetVkDevice();

	class PENGINE_API VulkanDevice : public Device
	{
	public:
#ifdef NDEBUG
		const bool enableValidationLayers = false;
#else
		const bool enableValidationLayers = true;
#endif
		VulkanDevice(const std::string& applicationName);
		virtual ~VulkanDevice() override;
		VulkanDevice(const VulkanDevice&) = delete;
		VulkanDevice& operator=(const VulkanDevice&) = delete;

		virtual void ShutDown() override;

		[[nodiscard]] virtual const std::string GetName() const override { return m_PhysicalDeviceProperties.deviceName; }

		[[nodiscard]] VkCommandPool GetCommandPool() const { return m_CommandPool; }

		[[nodiscard]] VkDevice GetDevice() const { return m_Device; }

		[[nodiscard]] VkInstance GetInstance() const { return m_Instance; }

		[[nodiscard]] VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }

		[[nodiscard]] uint32_t GetGraphicsFamilyIndex() const { return m_GraphicsFamilyIndex; }

		[[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

		[[nodiscard]] SwapChainSupportDetails GetSwapChainSupport(VkSurfaceKHR surface) const { return QuerySwapChainSupport(m_PhysicalDevice, surface); }

		[[nodiscard]] uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

		[[nodiscard]] QueueFamilyIndices FindPhysicalQueueFamilies() const { return FindQueueFamilies(m_PhysicalDevice); }

		[[nodiscard]] VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates,
			VkImageTiling tiling, VkFormatFeatureFlags features) const;

		[[nodiscard]] uint32_t GetVulkanApiVersion() const { return m_ApiVersion; }

		[[nodiscard]] VmaAllocator GetVmaAllocator() const { return m_VmaAllocator; }

		[[nodiscard]] std::shared_ptr<VulkanDescriptorPool> GetDescriptorPool() const { return m_DescriptorPool; }

		void CreateBuffer(
			VkDeviceSize size,
			VkBufferUsageFlags bufferUsage,
			VmaMemoryUsage memoryUsage,
			VmaAllocationCreateFlags memoryFlags,
			VkBuffer& buffer,
			VmaAllocation& vmaAllocation,
			VmaAllocationInfo& vmaAllocationInfo,
			VkDeviceSize minAlignment = 0) const;

		[[nodiscard]] uint32_t GetMinScratchOffsetAlignment() const { return m_MinScratchOffsetAlignment; }

		void DestroyBuffer(
			VkBuffer buffer,
			VmaAllocation vmaAllocation,
			VmaAllocationInfo vmaAllocationInfo) const;

		virtual void* Begin() override;

		virtual void End(void* frame) override;

		virtual void* CreateFrame() override;

		virtual void BeginFrame(void* frame) override;

		virtual void EndFrame(void* frame) override;

		virtual void DestroyFrame(void* frame) override;

		[[nodiscard]] VkCommandBuffer BeginSingleTimeCommands() const;

		void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const;

		void CopyBuffer(
			VkCommandBuffer commandBuffer,
			VkBuffer srcBuffer,
			VkBuffer dstBuffer,
			VkDeviceSize size,
			VkDeviceSize srcOffset = 0,
			VkDeviceSize dstOffset = 0) const;

		void CopyBufferToImage(
			VkBuffer buffer,
			VkImage image,
			uint32_t width,
			uint32_t height);

		void CopyImageToImage(
			VkImage src,
			VkImageLayout srcImageLayout,
			VkImage dst,
			VkImageLayout dstImageLayout,
			glm::ivec3 srcOffset,
			glm::ivec3 dstOffset,
			glm::uvec3 extent,
			VkCommandBuffer commandBuffer);

		void CreateImage(
			const VkImageCreateInfo& imageInfo,
			VmaMemoryUsage memoryUsage,
			VmaAllocationCreateFlags memoryFlags,
			VkImage& image,
			VmaAllocation& vmaAllocation,
			VmaAllocationInfo& vmaAllocationInfo) const;

		void DestroyImage(
			VkImage image,
			VmaAllocation allocation,
			VmaAllocationInfo vmaAllocationInfo);

		void TransitionImageLayout(
			VkImage image,
			VkFormat format,
			VkImageAspectFlagBits aspectMask,
			uint32_t mipLevels,
			uint32_t layerCount,
			VkImageLayout oldLayout,
			VkImageLayout newLayout,
			VkCommandBuffer commandBuffer) const;

		void GenerateMipMaps(
			VkImage image,
			VkFormat imageFormat,
			int32_t texWidth,
			int32_t texHeight,
			uint32_t mipLevels,
			uint32_t layerCount,
			VkCommandBuffer commandBuffer) const;

		void CommandBeginLabel(
			const std::string& name,
			VkCommandBuffer commandBuffer,
			const glm::vec3& color) const;

		void ClearDepthStencilImage(
			VkImage image,
			VkImageLayout imageLayout,
			const VkClearDepthStencilValue* depthStencilClearValue,
			uint32_t rangeCount,
			const VkImageSubresourceRange* ranges,
			VkCommandBuffer commandBuffer);

		void CommandEndLabel(VkCommandBuffer commandBuffer) const;

		virtual void WaitIdle() const override;

		void CreateAccelerationStructure(
			const VkAccelerationStructureCreateInfoKHR& createInfo,
			VkAccelerationStructureKHR& accelerationStructure) const;

		void DestroyAccelerationStructure(VkAccelerationStructureKHR accelerationStructure) const;

		void GetAccelerationStructureBuildSizes(
			VkAccelerationStructureBuildTypeKHR buildType,
			const VkAccelerationStructureBuildGeometryInfoKHR& buildInfo,
			const uint32_t* maxPrimitiveCounts,
			VkAccelerationStructureBuildSizesInfoKHR& sizeInfo) const;

		void CmdBuildAccelerationStructures(
			VkCommandBuffer commandBuffer,
			uint32_t infoCount,
			const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
			const VkAccelerationStructureBuildRangeInfoKHR* const* ppRangeInfos) const;

		VkDeviceAddress GetAccelerationStructureDeviceAddress(
			VkAccelerationStructureKHR accelerationStructure) const;

		void DeleteResource(std::function<void()>&& callback);

		virtual void FlushDeletionQueue(bool immediate = false) override;

		virtual void ForEachFrame(const std::function<void()>& callback) override;

		VkCommandBuffer GetCommandBufferFromFrame(void* frame);

		VkSurfaceKHR CreateSurface(GLFWwindow* window);

		VkCommandPool CreateCommandPool();

		VkCommandBuffer CreateCommandBuffer(VkCommandPool commandPool) const;

		VkFence CreateFence() const;

		void FreeCommandBuffer(VkCommandPool commandPool, VkCommandBuffer commandBuffer) const;
	private:

		void CreateInstance(const std::string& applicationName);

		void SetupDebugMessenger();

		void SetupDebugUtilsLabel();

		void PickPhysicalDevice();

		void CreateLogicalDevice();

		void CreateVmaAllocator();

		bool IsDeviceSuitable(VkPhysicalDevice device);

		[[nodiscard]] std::vector<const char*> GetRequiredExtensions() const;

		[[nodiscard]] bool CheckValidationLayerSupport() const;

		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;

		static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

		void HasGflwRequiredInstanceExtensions() const;

		bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

		[[nodiscard]] SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const;

		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger{};
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties m_PhysicalDeviceProperties{};
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		VkDevice m_Device = VK_NULL_HANDLE;
		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		uint32_t m_GraphicsFamilyIndex{};

		uint32_t m_ApiVersion = VK_API_VERSION_1_3;
		uint32_t m_MinScratchOffsetAlignment = 128;

		VmaAllocator m_VmaAllocator = VK_NULL_HANDLE;

		PFN_vkCmdBeginDebugUtilsLabelEXT m_VkCmdBeginDebugUtilsLabelEXT;
		PFN_vkCmdEndDebugUtilsLabelEXT m_VkCmdEndDebugUtilsLabelEXT;

		PFN_vkCreateAccelerationStructureKHR           m_vkCreateAccelerationStructureKHR = nullptr;
		PFN_vkDestroyAccelerationStructureKHR          m_vkDestroyAccelerationStructureKHR = nullptr;
		PFN_vkGetAccelerationStructureBuildSizesKHR    m_vkGetAccelerationStructureBuildSizesKHR = nullptr;
		PFN_vkCmdBuildAccelerationStructuresKHR        m_vkCmdBuildAccelerationStructuresKHR = nullptr;
		PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR = nullptr;

		const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
		const std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
			VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
			VK_KHR_RAY_QUERY_EXTENSION_NAME
		};

		mutable std::recursive_mutex m_Mutex;

		std::unordered_map<size_t, std::deque<std::function<void()>>> m_DeletionQueue;

		std::shared_ptr<VulkanDescriptorPool> m_DescriptorPool = nullptr;
		
		mutable bool m_SingleTimeCommandChecker = false;

	public:
		class Lock
		{
		public:
			Lock()
			{
				GetVkDevice()->m_Mutex.lock();
			}

			~Lock()
			{
				GetVkDevice()->m_Mutex.unlock();
			}
		};
	};

}
