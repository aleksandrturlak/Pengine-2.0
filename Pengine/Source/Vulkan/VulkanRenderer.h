#pragma once

#include "../Core/Core.h"
#include "../Graphics/Buffer.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Mesh.h"

#include <imgui/backends/imgui_impl_vulkan.h>

namespace Pengine::Vk
{

	class PENGINE_API VulkanRenderer final : public Renderer
	{
	public:
		VulkanRenderer();
		virtual ~VulkanRenderer() override = default;
		VulkanRenderer(const VulkanRenderer&) = delete;
		VulkanRenderer& operator=(const VulkanRenderer&) = delete;

		virtual void BeginRenderPass(
			const RenderPass::SubmitInfo& renderPassSubmitInfo,
			const std::string& debugName = {},
			const glm::vec3& debugColor = topLevelRenderPassDebugColor) override;

		virtual void EndRenderPass(const RenderPass::SubmitInfo& renderPassSubmitInfo) override;

		virtual void SetScissors(const RenderPass::Scissors& scissors, void* frame) override;

		virtual void SetViewport(const RenderPass::Viewport& viewport, void* frame) override;

		virtual void Render(
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
			void* frame) override;

		virtual void Compute(
			const std::shared_ptr<Pipeline>& pipeline,
			const glm::uvec3& groupCount,
			const std::vector<NativeHandle>& uniformWriters,
			void* frame) override;

		virtual void BindPipeline(
			const std::shared_ptr<Pipeline>& pipeline,
			void* frame) override;

		virtual void BindUniformWriters(
			const std::shared_ptr<Pipeline>& pipeline,
			const std::vector<NativeHandle>& uniformWriters,
			uint32_t offset,
			void* frame) override;

		virtual void BindVertexBuffers(
			std::vector<NativeHandle>& vertexBuffers,
			std::vector<size_t>& vertexBufferOffsets,
			const NativeHandle indexBuffer,
			const size_t indexBufferOffset,
			const NativeHandle instanceBuffer,
			const size_t instanceBufferOffset,
			void* frame) override;

		virtual void Draw(
			const uint32_t vertexCount,
			const uint32_t instanceCount,
			const uint32_t firstVertex,
			const uint32_t firstInstance,
			void* frame) override;

		virtual void DrawIndexed(
			const uint32_t indexCount,
    		const uint32_t instanceCount,
    		const uint32_t firstIndex,
    		const int32_t vertexOffset,
    		const uint32_t firstInstance,
			void* frame) override;

		virtual void DrawIndirectCount(
			const NativeHandle indirectBuffer,
			const size_t offset,
			const NativeHandle countBuffer,
			const size_t countBufferOffset,
			const uint32_t maxDrawCount,
			void* frame) override;

		virtual void Dispatch(
			const glm::uvec3& groupCount,
			void* frame) override;

		virtual void MemoryBarrierFragmentReadWrite(void* frame) override;

		virtual void MemoryBarrierVertexReadWrite(void* frame) override;
		
		virtual void MemoryBufferBarrierVertexReadWrite(NativeHandle buffer, void* frame) override;

		virtual void BeginCommandLabel(
			const std::string& name,
			const glm::vec3& color,
			void* frame) override;

		virtual void EndCommandLabel(void* frame) override;

		virtual void ClearDepthStencilImage(
			std::shared_ptr<Texture> texture,
			const RenderPass::ClearDepth& clearDepth,
			void* frame) override;

		virtual void FillBuffer(
			NativeHandle buffer,
			const size_t size,
			const size_t offset,
			uint32_t value,
			void* frame) override;
	};

}
