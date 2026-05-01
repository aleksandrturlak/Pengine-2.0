#pragma once

#include "Core.h"
#include "CustomData.h"

#include "../Graphics/Buffer.h"
#include "../Graphics/Texture.h"
#include "../Graphics/Pipeline.h"
#include "../Graphics/RenderPass.h"
#include "../Graphics/UniformWriter.h"

namespace Pengine
{

	class PENGINE_API UIRenderer : public CustomData
	{
	public:
		UIRenderer();
		virtual ~UIRenderer() override;

		void DrawQuad(
			const glm::vec2& position,
			const glm::vec2& size,
			const glm::vec2& uvMin,
			const glm::vec2& uvMax,
			const glm::vec4& color,
			const glm::vec4& cornerRadius,
			const glm::mat4& projectionMat4,
			void* texture,
			const bool isText);

		void Render(
			const std::vector<entt::entity>& entities,
			std::shared_ptr<class BaseMaterial> baseMaterial,
			const RenderPass::RenderCallbackInfo& renderInfo);

private:
		void RecordPreviousDrawCommand(const uint32_t batchIndex, std::shared_ptr<Pipeline> pipeline);

		void RenderBatch(
			uint32_t& batchIndex,
			const RenderPass::RenderCallbackInfo& renderInfo,
			std::shared_ptr<Pipeline> pipeline);

		void ProcessCommand(
			const RenderPass::RenderCallbackInfo& renderInfo,
			const void* command,
			uint32_t& batchIndex,
			std::shared_ptr<Pipeline> pipeline,
			const glm::mat4& projectionMat4);

		struct QuadVertex
		{
			glm::vec4 position;
			glm::vec2 positionInPixels;
			glm::vec2 uv;
			float cornerRadius;
			int instanceIndex;
		};

		struct QuadInstance
		{
			glm::vec4 color;
			glm::vec4 quadCenterAndHalfSize;

			/**
			 * { isText, empty, empty. empty }
			 */
			glm::ivec4 customData;
		};

		struct Batch
		{
			std::shared_ptr<Buffer> vertexBuffer;
			std::shared_ptr<Buffer> indexBuffer;
			std::shared_ptr<Buffer> uniformBuffer;
			std::shared_ptr<UniformWriter> uniformWriter;

			struct DrawCommand
			{
				size_t indexBufferOffset = 0;
				int indexCount = 0;

				uint32_t quadCount = 0;
				void* textureId = nullptr;

				std::optional<RenderPass::Scissors> scissors;
			};

			std::vector<DrawCommand> drawCommands;
		};

		std::vector<Batch> m_Batches;

		Batch& GetOrCreateBatch(const uint32_t batchIndex, std::shared_ptr<Pipeline> pipeline);

		std::vector<QuadInstance> m_QuadInstances;
		std::vector<QuadVertex> m_QuadVertices;
		std::vector<uint32_t> m_QuadIndices;
		uint32_t m_QuadCount = 0;
		void* m_CurrentTextureId = nullptr;
		void* m_WhiteTexture = nullptr;
		std::optional<RenderPass::Scissors> m_Scissors;
	};

}
