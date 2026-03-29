#include "VulkanRenderPass.h"

#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "VulkanFormat.h"

#include "../Core/Logger.h"

using namespace Pengine;
using namespace Vk;

VulkanRenderPass::VulkanRenderPass(const CreateInfo& createInfo)
	: RenderPass(createInfo)
{
	std::vector<VkAttachmentReference> colorAttachmentRefs;
	std::optional<VkAttachmentReference> depthAttachmentRef;

	size_t attachmentIndex = 0;
	std::vector<VkAttachmentDescription> attachments(createInfo.attachmentDescriptions.size());
	for (auto const& attachmentDescription : createInfo.attachmentDescriptions)
	{
		VkAttachmentDescription attachment{};
		attachment.format = ConvertFormat(attachmentDescription.textureCreateInfo.format);
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = (VkAttachmentLoadOp)attachmentDescription.load;
		attachment.storeOp = (VkAttachmentStoreOp)attachmentDescription.store;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

		attachments[attachmentIndex] = attachment;

		VkAttachmentReference attachmentRef{};
		attachmentRef.attachment = attachmentIndex;
		attachmentRef.layout = VulkanTexture::ConvertLayout(attachmentDescription.layout);
		if (attachmentDescription.layout == Texture::Layout::COLOR_ATTACHMENT_OPTIMAL)
		{
			colorAttachmentRefs.emplace_back(attachmentRef);
		}
		else if (attachmentDescription.layout == Texture::Layout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			depthAttachmentRef = attachmentRef;
		}

		attachmentIndex++;
	}

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colorAttachmentRefs.size();
	subpass.pColorAttachments = colorAttachmentRefs.data();
	subpass.pDepthStencilAttachment = depthAttachmentRef ? &*depthAttachmentRef : VK_NULL_HANDLE;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	dependency.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	dependency.dstSubpass = 0;
	dependency.dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
	dependency.dstAccessMask =
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassCreateInfo.pAttachments = attachments.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(GetVkDevice()->GetDevice(), &renderPassCreateInfo,
		nullptr, &m_RenderPass) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to create render pass!");
	}
}

VulkanRenderPass::~VulkanRenderPass()
{
	GetVkDevice()->DeleteResource([renderPass = m_RenderPass]()
	{
		vkDestroyRenderPass(GetVkDevice()->GetDevice(), renderPass, nullptr);
	});
}
