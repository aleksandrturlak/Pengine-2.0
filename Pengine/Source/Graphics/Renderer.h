#pragma once

#include "../Core/Core.h"
#include "../Graphics/BarrierBatch.h"
#include "../Graphics/Buffer.h"
#include "../Graphics/FrameBuffer.h"
#include "../Graphics/RenderPass.h"
#include "../Graphics/UniformWriter.h"

namespace Pengine
{

	class Mesh;
	class Window;
	class Camera;
	class Scene;
	class Pipeline;
	class Entity;

	class PENGINE_API Renderer
	{
	public:
		struct RenderViewportInfo
		{
			std::shared_ptr<Entity> camera;
			std::shared_ptr<RenderView> renderView;
			glm::mat4 projection;
			glm::ivec2 size;
		};

		static std::shared_ptr<Renderer> Create();

		Renderer() = default;
		virtual ~Renderer() = default;
		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) = delete;

		static void Update(
			void* frame,
			const std::shared_ptr<Window>& window,
			const std::shared_ptr<Renderer>& renderer,
			const std::map<std::shared_ptr<Scene>, std::vector<RenderViewportInfo>>& viewportsByScene);

		virtual void BeginRenderPass(
			const RenderPass::SubmitInfo& renderPassSubmitInfo,
			const std::string& debugName = {},
			const glm::vec3& debugColor = topLevelRenderPassDebugColor) = 0;

		virtual void EndRenderPass(const RenderPass::SubmitInfo& renderPassSubmitInfo) = 0;

		virtual void SetScissors(const RenderPass::Scissors& scissors, void* frame) = 0;

		virtual void SetViewport(const RenderPass::Viewport& viewport, void* frame) = 0;

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
			void* frame) = 0;

		virtual void Compute(
			const std::shared_ptr<Pipeline>& pipeline,
			const glm::uvec3& groupCount,
			const std::vector<NativeHandle>& uniformWriters,
			void* frame) = 0;

		virtual void BindPipeline(
			const std::shared_ptr<Pipeline>& pipeline,
			void* frame) = 0;

		virtual void BindUniformWriters(
			const std::shared_ptr<Pipeline>& pipeline,
			const std::vector<NativeHandle>& uniformWriters,
			uint32_t offset,
			void* frame) = 0;

		virtual void BindVertexBuffers(
			std::vector<NativeHandle>& vertexBuffers,
			std::vector<size_t>& vertexBufferOffsets,
			const NativeHandle indexBuffer,
			const size_t indexBufferOffset,
			const NativeHandle instanceBuffer,
			const size_t instanceBufferOffset,
			void* frame) = 0;

		virtual void Draw(
			const uint32_t vertexCount,
			const uint32_t instanceCount,
			const uint32_t firstVertex,
			const uint32_t firstInstance,
			void* frame) = 0;
			
		virtual void DrawIndexed(
			const uint32_t indexCount,
    		const uint32_t instanceCount,
    		const uint32_t firstIndex,
    		const int32_t vertexOffset,
    		const uint32_t firstInstance,
			void* frame) = 0;

		virtual void DrawIndirectCount(
			const NativeHandle indirectBuffer,
			const size_t offset,
			const NativeHandle countBuffer,
			const size_t countBufferOffset,
			const uint32_t maxDrawCount,
			void* frame) = 0;

		virtual void Dispatch(
			const glm::uvec3& groupCount,
			void* frame) = 0;

		virtual void PipelineBarrier(const BarrierBatch& batch, void* frame) = 0;

		virtual void BeginCommandLabel(
			const std::string& name,
			const glm::vec3& color,
			void* frame) = 0;

		virtual void EndCommandLabel(void* frame) = 0;

		virtual void ClearDepthStencilImage(
			std::shared_ptr<Texture> texture,
			const RenderPass::ClearDepth& clearDepth,
			void* frame) = 0;

		virtual void ClearColorImage(
			std::shared_ptr<Texture> texture,
			const glm::vec4& clearColor,
			void* frame) = 0;

		virtual void FillBuffer(
			NativeHandle buffer,
			const size_t size,
			const size_t offset,
			uint32_t value,
			void* frame) = 0;

		virtual void PushConstants(
			const std::shared_ptr<Pipeline>& pipeline,
			ShaderStage stageFlags,
			uint32_t offset,
			uint32_t size,
			const void* data,
			void* frame) = 0;
	};

}