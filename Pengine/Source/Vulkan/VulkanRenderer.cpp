#include "VulkanRenderer.h"

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanFrameBuffer.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanComputePipeline.h"
#include "VulkanRenderPass.h"
#include "VulkanTexture.h"
#include "VulkanUniformWriter.h"
#include "VulkanWindow.h"

#include "../Core/Profiler.h"

using namespace Pengine;
using namespace Vk;

VulkanRenderer::VulkanRenderer()
	: Renderer()
{

}

void VulkanRenderer::Render(
	std::vector<NativeHandle>& vertexBuffers,
	std::vector<size_t>& vertexBufferOffsets,
	const NativeHandle indexBuffer,
	const size_t indexBufferOffset,
	const uint32_t indexCount,
	const std::shared_ptr<Pipeline>& pipeline,
	const NativeHandle instanceBuffer,
	const size_t instanceBufferOffset,
	const uint32_t count,
	const std::vector<NativeHandle>& uniformWriters,
	void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	
	const std::shared_ptr<VulkanGraphicsPipeline>& vkPipeline = std::static_pointer_cast<VulkanGraphicsPipeline>(pipeline);
	vkCmdBindPipeline(vkFrame->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->GetPipeline());

	if (!uniformWriters.empty())
	{
		PROFILER_SCOPE("BindDescriptorSets");

		vkCmdBindDescriptorSets(
			vkFrame->CommandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			vkPipeline->GetPipelineLayout(),
			0,
			uniformWriters.size(),
			(VkDescriptorSet*)uniformWriters.data(),
			0,
			nullptr);
	}

	BindVertexBuffers(vertexBuffers, vertexBufferOffsets, indexBuffer, indexBufferOffset, instanceBuffer, instanceBufferOffset, frame);
	DrawIndexed(indexCount, count, 0, 0, 0, frame);
}

void VulkanRenderer::Compute(
	const std::shared_ptr<Pipeline>& pipeline,
	const glm::uvec3& groupCount,
	const std::vector<NativeHandle>& uniformWriters,
	void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	const std::shared_ptr<VulkanComputePipeline>& vkPipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(pipeline);
	vkCmdBindPipeline(vkFrame->CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline->GetPipeline());

	if (!uniformWriters.empty())
	{
		vkCmdBindDescriptorSets(
			vkFrame->CommandBuffer,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			vkPipeline->GetPipelineLayout(),
			0,
			uniformWriters.size(),
			(VkDescriptorSet*)uniformWriters.data(),
			0,
			nullptr);
	}

	vkCmdDispatch(vkFrame->CommandBuffer, groupCount.x, groupCount.y, groupCount.z);
}

void VulkanRenderer::BindPipeline(
	const std::shared_ptr<Pipeline>& pipeline,
	void* frame)
{
	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	switch (pipeline->GetType())
	{
	case Pipeline::Type::GRAPHICS:
		vkCmdBindPipeline(
			vkFrame->CommandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			std::static_pointer_cast<VulkanGraphicsPipeline>(pipeline)->GetPipeline());
		break;
	case Pipeline::Type::COMPUTE:
		vkCmdBindPipeline(
			vkFrame->CommandBuffer,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			std::static_pointer_cast<VulkanComputePipeline>(pipeline)->GetPipeline());
		break;
	}
}

void VulkanRenderer::BindUniformWriters(
	const std::shared_ptr<Pipeline>& pipeline,
	const std::vector<NativeHandle>& uniformWriters,
	uint32_t offset,
	void* frame)
{
	if (uniformWriters.empty())
	{
		return;
	}

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	VkPipelineLayout vkPipelineLayout{};
	VkPipelineBindPoint vkPipelineBindPoint{};
	if (pipeline->GetType() == Pipeline::Type::GRAPHICS)
	{
		const std::shared_ptr<VulkanGraphicsPipeline>& vkGraphicsPipeline = std::static_pointer_cast<VulkanGraphicsPipeline>(pipeline);
		vkPipelineLayout = vkGraphicsPipeline->GetPipelineLayout();
		vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	}
	else if (pipeline->GetType() == Pipeline::Type::COMPUTE)
	{
		const std::shared_ptr<VulkanComputePipeline>& vkComputePipeline = std::static_pointer_cast<VulkanComputePipeline>(pipeline);
		vkPipelineLayout = vkComputePipeline->GetPipelineLayout();
		vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	}

	vkCmdBindDescriptorSets(
		vkFrame->CommandBuffer,
		vkPipelineBindPoint,
		vkPipelineLayout,
		offset,
		uniformWriters.size(),
		(VkDescriptorSet*)uniformWriters.data(),
		0,
		nullptr);
}

void VulkanRenderer::BindVertexBuffers(
	std::vector<NativeHandle> &vertexBuffers,
	std::vector<size_t> &vertexBufferOffsets,
	const NativeHandle indexBuffer,
	const size_t indexBufferOffset,
	const NativeHandle instanceBuffer,
	const size_t instanceBufferOffset,
	void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	assert(vertexBuffers.size() == vertexBufferOffsets.size());

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	if (instanceBuffer)
	{
		vertexBuffers.emplace_back(instanceBuffer);
		vertexBufferOffsets.emplace_back(instanceBufferOffset);
	}

	vkCmdBindVertexBuffers(vkFrame->CommandBuffer, 0, vertexBuffers.size(), (VkBuffer*)vertexBuffers.data(), (VkDeviceSize*)vertexBufferOffsets.data());
	vkCmdBindIndexBuffer(vkFrame->CommandBuffer, *(VkBuffer*)&indexBuffer, indexBufferOffset, VK_INDEX_TYPE_UINT32);
}

void VulkanRenderer::Draw(
	const uint32_t vertexCount,
	const uint32_t instanceCount,
	const uint32_t firstVertex,
	const uint32_t firstInstance,
	void *frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	vkCmdDraw(vkFrame->CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	drawCallCount++;
	triangleCount += (vertexCount / 3) * instanceCount;
}

void VulkanRenderer::DrawIndexed(
	const uint32_t indexCount,
	const uint32_t instanceCount,
	const uint32_t firstIndex,
	const int32_t vertexOffset,
	const uint32_t firstInstance,
	void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	vkCmdDrawIndexed(vkFrame->CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	drawCallCount++;
	triangleCount += (indexCount / 3) * instanceCount;
}

void VulkanRenderer::DrawIndirectCount(
	const NativeHandle indirectBuffer,
	const size_t offset,
	const NativeHandle countBuffer,
	const size_t countBufferOffset,
	const uint32_t maxDrawCount,
	void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	vkCmdDrawIndirectCount(
		vkFrame->CommandBuffer,
		*(VkBuffer*)&indirectBuffer,
		offset * sizeof(VkDrawIndirectCommand),
		*(VkBuffer*)&countBuffer,
		countBufferOffset * sizeof(uint32_t),
		maxDrawCount,
		sizeof(VkDrawIndirectCommand));
}

void VulkanRenderer::Dispatch(
	const glm::uvec3& groupCount,
	void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	vkCmdDispatch(vkFrame->CommandBuffer, groupCount.x, groupCount.y, groupCount.z);
}

static VkPipelineStageFlags ToVkStage(PipelineStage stage)
{
	VkPipelineStageFlags flags = 0;
	if (static_cast<uint32_t>(stage & PipelineStage::DrawIndirect))    flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::VertexInput))     flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::VertexShader))    flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::FragmentShader))  flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::ColorAttachment)) flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::EarlyDepth))      flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::LateDepth))       flags |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::ComputeShader))   flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::Transfer))        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::Host))            flags |= VK_PIPELINE_STAGE_HOST_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::AllGraphics))              flags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::AllCommands))              flags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	if (static_cast<uint32_t>(stage & PipelineStage::AccelerationStructureBuild)) flags |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	return flags;
}

static VkAccessFlags ToVkAccess(Access access)
{
	VkAccessFlags flags = 0;
	if (static_cast<uint32_t>(access & Access::IndirectCommandRead))  flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	if (static_cast<uint32_t>(access & Access::VertexAttributeRead))  flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	if (static_cast<uint32_t>(access & Access::UniformRead))          flags |= VK_ACCESS_UNIFORM_READ_BIT;
	if (static_cast<uint32_t>(access & Access::ShaderRead))           flags |= VK_ACCESS_SHADER_READ_BIT;
	if (static_cast<uint32_t>(access & Access::ShaderWrite))          flags |= VK_ACCESS_SHADER_WRITE_BIT;
	if (static_cast<uint32_t>(access & Access::ColorAttachmentRead))  flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	if (static_cast<uint32_t>(access & Access::ColorAttachmentWrite)) flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	if (static_cast<uint32_t>(access & Access::DepthAttachmentRead))  flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	if (static_cast<uint32_t>(access & Access::DepthAttachmentWrite)) flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	if (static_cast<uint32_t>(access & Access::TransferRead))         flags |= VK_ACCESS_TRANSFER_READ_BIT;
	if (static_cast<uint32_t>(access & Access::TransferWrite))        flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
	if (static_cast<uint32_t>(access & Access::HostRead))             flags |= VK_ACCESS_HOST_READ_BIT;
	if (static_cast<uint32_t>(access & Access::HostWrite))            flags |= VK_ACCESS_HOST_WRITE_BIT;
	if (static_cast<uint32_t>(access & Access::MemoryRead))                    flags |= VK_ACCESS_MEMORY_READ_BIT;
	if (static_cast<uint32_t>(access & Access::MemoryWrite))                   flags |= VK_ACCESS_MEMORY_WRITE_BIT;
	if (static_cast<uint32_t>(access & Access::AccelerationStructureRead))     flags |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	if (static_cast<uint32_t>(access & Access::AccelerationStructureWrite))    flags |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	return flags;
}

static VkImageLayout ToVkLayout(ImageLayout layout)
{
	switch (layout)
	{
	case ImageLayout::Undefined:        return VK_IMAGE_LAYOUT_UNDEFINED;
	case ImageLayout::General:          return VK_IMAGE_LAYOUT_GENERAL;
	case ImageLayout::ColorAttachment:  return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case ImageLayout::DepthAttachment:  return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case ImageLayout::ShaderReadOnly:   return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case ImageLayout::TransferSrc:      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case ImageLayout::TransferDst:      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case ImageLayout::Present:          return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	default:                            return VK_IMAGE_LAYOUT_UNDEFINED;
	}
}

void VulkanRenderer::PipelineBarrier(const BarrierBatch& batch, void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	std::vector<VkMemoryBarrier> memoryBarriers;
	memoryBarriers.reserve(batch.memory.size());
	for (const MemoryBarrierDesc& memory : batch.memory)
	{
		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = ToVkAccess(memory.srcAccess);
		barrier.dstAccessMask = ToVkAccess(memory.dstAccess);
		memoryBarriers.push_back(barrier);
	}

	std::vector<VkBufferMemoryBarrier> bufferBarriers;
	bufferBarriers.reserve(batch.buffers.size());
	for (const BufferBarrierDesc& buffer : batch.buffers)
	{
		VkBufferMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask = ToVkAccess(buffer.srcAccess);
		barrier.dstAccessMask = ToVkAccess(buffer.dstAccess);
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = *(VkBuffer*)&buffer.buffer;
		barrier.offset = buffer.offset;
		barrier.size = buffer.size;
		bufferBarriers.push_back(barrier);
	}

	std::vector<VkImageMemoryBarrier> imageBarriers;
	imageBarriers.reserve(batch.images.size());
	for (const ImageBarrierDesc& image : batch.images)
	{
		const std::shared_ptr<VulkanTexture> vkTexture = std::static_pointer_cast<VulkanTexture>(image.texture);
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = ToVkAccess(image.srcAccess);
		barrier.dstAccessMask = ToVkAccess(image.dstAccess);
		barrier.oldLayout = ToVkLayout(image.oldLayout);
		barrier.newLayout = ToVkLayout(image.newLayout);
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = vkTexture->GetImage();
		barrier.subresourceRange.aspectMask = VulkanTexture::ConvertAspectMask(vkTexture->GetAspectMask());
		barrier.subresourceRange.baseMipLevel = image.baseMip;
		barrier.subresourceRange.levelCount = image.mipCount;
		barrier.subresourceRange.baseArrayLayer = image.baseLayer;
		barrier.subresourceRange.layerCount = image.layerCount;
		imageBarriers.push_back(barrier);
	}

	vkCmdPipelineBarrier(
		vkFrame->CommandBuffer,
		ToVkStage(batch.srcStage),
		ToVkStage(batch.dstStage),
		0,
		static_cast<uint32_t>(memoryBarriers.size()), memoryBarriers.data(),
		static_cast<uint32_t>(bufferBarriers.size()), bufferBarriers.data(),
		static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());
}

void VulkanRenderer::BeginCommandLabel(
	const std::string& name,
	const glm::vec3& color,
	void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);
	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	GetVkDevice()->CommandBeginLabel(name, vkFrame->CommandBuffer, color);
}

void VulkanRenderer::EndCommandLabel(void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);
	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	GetVkDevice()->CommandEndLabel(vkFrame->CommandBuffer);
}

void VulkanRenderer::ClearDepthStencilImage(
	std::shared_ptr<Texture> texture,
	const RenderPass::ClearDepth& clearDepth,
	void* frame)
{
	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	const std::shared_ptr<VulkanTexture> vkTexture = std::static_pointer_cast<VulkanTexture>(texture);
	
	VkClearDepthStencilValue clearValue{};
	clearValue.depth = clearDepth.clearDepth;
	clearValue.stencil = clearDepth.clearStencil;
	
	VkImageSubresourceRange range{};
	range.aspectMask = VulkanTexture::ConvertAspectMask(vkTexture->GetAspectMask());
	range.baseMipLevel = 0;
	range.levelCount = vkTexture->GetMipLevels();
	range.baseArrayLayer = 0;
	range.layerCount = vkTexture->GetLayerCount();

	GetVkDevice()->ClearDepthStencilImage(vkTexture->GetImage(), vkTexture->GetLayout(), &clearValue, 1, &range, vkFrame->CommandBuffer);
}

void VulkanRenderer::FillBuffer(
	NativeHandle buffer,
	const size_t size,
	const size_t offset,
	uint32_t value,
	void *frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	vkCmdFillBuffer(vkFrame->CommandBuffer, *(VkBuffer*)&buffer, offset, size, value);
}

void VulkanRenderer::PushConstants(
	const std::shared_ptr<Pipeline>& pipeline,
	ShaderStage stageFlags,
	uint32_t offset,
	uint32_t size,
	const void* data,
	void* frame)
{
	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	const std::shared_ptr<VulkanComputePipeline>& vkPipeline = std::static_pointer_cast<VulkanComputePipeline>(pipeline);
	vkCmdPushConstants(vkFrame->CommandBuffer, vkPipeline->GetPipelineLayout(),
		static_cast<VkShaderStageFlags>(stageFlags), offset, size, data);
}

void VulkanRenderer::BeginRenderPass(
	const RenderPass::SubmitInfo& renderPassSubmitInfo,
	const std::string& debugName,
	const glm::vec3& debugColor)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* frame = static_cast<VulkanFrameInfo*>(renderPassSubmitInfo.frame);

	if (!debugName.empty())
	{
		BeginCommandLabel(debugName, debugColor, renderPassSubmitInfo.frame);
	}
	else
	{
		BeginCommandLabel(renderPassSubmitInfo.renderPass->GetName(), debugColor, renderPassSubmitInfo.frame);
	}

	std::vector<VkClearValue> vkClearValues;
	for (const glm::vec4& clearColor : renderPassSubmitInfo.renderPass->GetClearColors())
	{
		VkClearValue clearValue{};

		clearValue.color.float32[0] = clearColor[0];
		clearValue.color.float32[1] = clearColor[1];
		clearValue.color.float32[2] = clearColor[2];
		clearValue.color.float32[3] = clearColor[3];
		vkClearValues.emplace_back(clearValue);
	}

	for (const RenderPass::ClearDepth& clearDepth : renderPassSubmitInfo.renderPass->GetClearDepth())
	{
		VkClearValue clearValue{};
		clearValue.depthStencil.depth = clearDepth.clearDepth;
		clearValue.depthStencil.stencil = clearDepth.clearStencil;
		vkClearValues.emplace_back(clearValue);
	}

	const glm::ivec2 size = renderPassSubmitInfo.frameBuffer->GetSize();

	VkRenderPassBeginInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = std::static_pointer_cast<VulkanRenderPass>(renderPassSubmitInfo.renderPass)->GetRenderPass();
	info.framebuffer = std::static_pointer_cast<VulkanFrameBuffer>(renderPassSubmitInfo.frameBuffer)->GetFrameBuffer();
	info.renderArea.extent.width = size.x;
	info.renderArea.extent.height = size.y;
	info.clearValueCount = vkClearValues.size();
	info.pClearValues = vkClearValues.data();
	vkCmdBeginRenderPass(frame->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	if (renderPassSubmitInfo.viewport)
	{
		viewport.x = renderPassSubmitInfo.viewport->position.x;
		viewport.y = renderPassSubmitInfo.viewport->position.y;
		viewport.width = renderPassSubmitInfo.viewport->size.x;
		viewport.height = renderPassSubmitInfo.viewport->size.y;
		viewport.minDepth = renderPassSubmitInfo.viewport->minMaxDepth.x;
		viewport.maxDepth = renderPassSubmitInfo.viewport->minMaxDepth.y;
	}
	else
	{
		viewport.x = 0;
		viewport.y = static_cast<float>(size.y);
		viewport.width = static_cast<float>(size.x);
		viewport.height = -static_cast<float>(size.y);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
	}

	VkRect2D scissor{};
	if (renderPassSubmitInfo.scissors)
	{
		scissor =
		{
			{ renderPassSubmitInfo.scissors->offset.x, renderPassSubmitInfo.scissors->offset.y },
			{ renderPassSubmitInfo.scissors->size.x, renderPassSubmitInfo.scissors->size.y }
		};
	}
	else
	{
		scissor = { { 0, 0 }, { (uint32_t)size.x, (uint32_t)size.y } };
	}

	vkCmdSetViewport(frame->CommandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(frame->CommandBuffer, 0, 1, &scissor);
}

void VulkanRenderer::EndRenderPass(const RenderPass::SubmitInfo& renderPassSubmitInfo)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* frame = static_cast<VulkanFrameInfo*>(renderPassSubmitInfo.frame);
	vkCmdEndRenderPass(frame->CommandBuffer);
	EndCommandLabel(renderPassSubmitInfo.frame);
}

void VulkanRenderer::SetScissors(const RenderPass::Scissors& scissors, void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VkRect2D scissor =
	{
		{ scissors.offset.x, scissors.offset.y },
		{ scissors.size.x, scissors.size.y }
	};

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	vkCmdSetScissor(vkFrame->CommandBuffer, 0, 1, &scissor);
}

void VulkanRenderer::SetViewport(const RenderPass::Viewport& viewport, void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	VkViewport vkViewport{};
	vkViewport.x = viewport.position.x;
	vkViewport.y = viewport.position.y;
	vkViewport.width = viewport.size.x;
	vkViewport.height =viewport.size.y;
	vkViewport.minDepth = viewport.minMaxDepth.x;
	vkViewport.maxDepth = viewport.minMaxDepth.y;

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	vkCmdSetViewport(vkFrame->CommandBuffer, 0, 1, &vkViewport);
}
