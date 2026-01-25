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
	vkPipeline->Bind(vkFrame->CommandBuffer);

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
	DrawIndexed(indexCount, count, frame);
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
	vkPipeline->Bind(vkFrame->CommandBuffer);

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
	std::static_pointer_cast<VulkanGraphicsPipeline>(pipeline)->Bind(vkFrame->CommandBuffer);
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

void VulkanRenderer::DrawIndexed(
	const uint32_t indexCount,
	const uint32_t instanceCount,
	void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);
	vkCmdDrawIndexed(vkFrame->CommandBuffer, indexCount, instanceCount, 0, 0, 0);
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

void VulkanRenderer::MemoryBarrierFragmentReadWrite(void* frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	VkMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vkCmdPipelineBarrier(
		vkFrame->CommandBuffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		1,
		&memoryBarrier,
		0,
		nullptr,
		0,
		nullptr);
}

void VulkanRenderer::MemoryBarrierVertexReadWrite(void *frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	VkMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(
		vkFrame->CommandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		0,
		1,
		&memoryBarrier,
		0,
		nullptr,
		0,
		nullptr);
}

void VulkanRenderer::MemoryBufferBarrierVertexReadWrite(NativeHandle buffer, void *frame)
{
	PROFILER_SCOPE(__FUNCTION__);

	const VulkanFrameInfo* vkFrame = static_cast<VulkanFrameInfo*>(frame);

	VkBufferMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	memoryBarrier.buffer = *(VkBuffer*)&buffer;
	memoryBarrier.offset = 0;
	memoryBarrier.size = VK_WHOLE_SIZE;
	vkCmdPipelineBarrier(
		vkFrame->CommandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		0,
		0, nullptr,
		1, &memoryBarrier,
		0, nullptr);
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
