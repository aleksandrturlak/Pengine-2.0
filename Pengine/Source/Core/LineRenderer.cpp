#include "LineRenderer.h"

#include "RenderPassManager.h"
#include "MaterialManager.h"
#include "Scene.h"
#include "Time.h"

#include "../Graphics/Renderer.h"

#define MAX_BATCH_LINE_COUNT 30000
#define LINE_VERTEX_COUNT 2
#define MAX_BATCH_LINE_VERTEX_COUNT MAX_BATCH_LINE_COUNT * LINE_VERTEX_COUNT

using namespace Pengine;

LineRenderer::~LineRenderer()
{
	m_Batches.clear();
}

void LineRenderer::Render(const RenderPass::RenderCallbackInfo& renderInfo)
{
	if (std::shared_ptr<BaseMaterial> lineBaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial("Materials/Line.basemat"))
	{
		std::shared_ptr<Pipeline> pipeline = lineBaseMaterial->GetPipeline(renderInfo.renderPass->GetName());
		std::vector<NativeHandle> uniformWritersNativeHandles;
		std::vector<std::shared_ptr<UniformWriter>> uniformWriters;
		RenderPassManager::GetUniformWriters(pipeline, lineBaseMaterial, nullptr, renderInfo, uniformWriters, uniformWritersNativeHandles);

		for (const auto& uniformWriter : uniformWriters)
		{
			uniformWriter->Flush();

			for (const auto& [location, buffers] : uniformWriter->GetBuffersByName())
			{
				for (const auto& buffer : buffers)
				{
					buffer->Flush();
				}
			}
		}

		auto render = [this, &uniformWritersNativeHandles, &renderInfo, &pipeline](
			uint32_t& index,
			uint32_t& batchIndex,
			std::vector<glm::vec3>& lineVertices,
			std::vector<uint32_t>& lineIndices)
		{
			if (m_Batches.size() == batchIndex)
			{
				Batch batch;
				batch.vertexBuffer = Buffer::Create(
					sizeof(glm::vec3) * 2,
					MAX_BATCH_LINE_COUNT * 2,
					{ Buffer::Usage::VERTEX_BUFFER },
					MemoryType::CPU,
					true);

				batch.indexBuffer = Buffer::Create(
					sizeof(uint32_t),
					MAX_BATCH_LINE_COUNT * 2,
					{ Buffer::Usage::INDEX_BUFFER },
					MemoryType::CPU,
					true);

				m_Batches.emplace_back(batch);
			}

			Batch& batch = m_Batches[batchIndex];

			batch.vertexBuffer->WriteToBuffer(lineVertices.data(), lineVertices.size() * sizeof(glm::vec3));
			batch.vertexBuffer->Flush();
			batch.indexBuffer->WriteToBuffer(lineIndices.data(), lineIndices.size() * sizeof(uint32_t));
			batch.indexBuffer->Flush();

			std::vector<NativeHandle> vertexBuffers = { batch.vertexBuffer->GetNativeHandle() };
			std::vector<size_t> vertexBufferOffsets = { 0 };

			renderInfo.renderer->Render(
				vertexBuffers,
				vertexBufferOffsets,
				batch.indexBuffer->GetNativeHandle(),
				0,
				index,
				pipeline,
				NativeHandle::Invalid(),
				0,
				1,
				uniformWritersNativeHandles,
				renderInfo.frame);

			batchIndex++;
			index = 0;
			lineVertices.clear();
			lineIndices.clear();
		};

		std::queue<Line>& lines = renderInfo.scene->GetVisualizer().GetLines();
		if (!lines.empty())
		{
			std::queue<Line> linesForTheNextFrame;

			std::vector<glm::vec3> lineVertices;
			std::vector<uint32_t> lineIndices;
			lineVertices.reserve(MAX_BATCH_LINE_VERTEX_COUNT * 2);
			lineIndices.reserve(MAX_BATCH_LINE_VERTEX_COUNT);
			uint32_t index = 0;
			uint32_t batchIndex = 0;
			for (; !lines.empty(); lines.pop())
			{
				if (index == MAX_BATCH_LINE_VERTEX_COUNT)
				{
					render(index, batchIndex, lineVertices, lineIndices);
				}

				Line& line = lines.front();
				lineVertices.emplace_back(line.start);
				lineVertices.emplace_back(line.color);
				lineVertices.emplace_back(line.end);
				lineVertices.emplace_back(line.color);

				lineIndices.emplace_back(index);
				index++;
				lineIndices.emplace_back(index);
				index++;

				line.duration -= (float)Time::GetDeltaTime();
				if (line.duration > 0.0f)
				{
					linesForTheNextFrame.emplace(line);
				}
			}

			lines = std::move(linesForTheNextFrame);

			render(index, batchIndex, lineVertices, lineIndices);
		}
	}
}
