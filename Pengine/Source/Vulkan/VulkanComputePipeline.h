#pragma once

#include "../Core/Core.h"
#include "../Graphics/ComputePipeline.h"

#include <vulkan/vulkan.h>

namespace Pengine::Vk
{

	class PENGINE_API VulkanComputePipeline final : public ComputePipeline
	{
	public:

		explicit VulkanComputePipeline(const CreateComputeInfo& createComputeInfo);
		virtual ~VulkanComputePipeline() override;
		VulkanComputePipeline(const VulkanComputePipeline&) = delete;
		VulkanComputePipeline& operator=(const VulkanComputePipeline&) = delete;

		VkPipeline GetPipeline() const { return m_ComputePipeline; }

		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

	private:
		VkPipeline m_ComputePipeline{};
		VkPipelineLayout m_PipelineLayout{};
	};

}
