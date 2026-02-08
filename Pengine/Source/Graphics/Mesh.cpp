#include "Mesh.h"

#include "../Core/Logger.h"
#include "../Core/Raycast.h"

using namespace Pengine;

Mesh::Mesh(const CreateInfo& createInfo)
	: Asset(createInfo.name, createInfo.filepath)
{
	Reload(createInfo);
}

Mesh::~Mesh()
{
	if (m_CreateInfo.vertices)
	{
		delete m_CreateInfo.vertices;
		m_CreateInfo.vertices = nullptr;
	}

	m_Vertices.clear();
}

const std::shared_ptr<Buffer>& Mesh::GetVertexBuffer(const size_t index) const
{
	// Kind of slow. If crashes uncomment this section.
	//std::lock_guard<std::mutex> lock(m_VertexBufferAccessMutex);

	/*if (index >= m_Vertices.size())
	{
		const std::string message = GetFilepath().string() + ":doesn't have enough vertex buffers!";
		FATAL_ERROR(message);
	}*/
	
	return m_Vertices[index];
}

const std::shared_ptr<Buffer> &Pengine::Mesh::GetMeshInfoBuffer() const
{
    return m_MeshInfoBuffer;
}

bool Mesh::Raycast(
	const glm::vec3& start,
	const glm::vec3& direction,
	const float length,
	Raycast::Hit& hit,
	Visualizer& visualizer) const
{
	return m_BVH->Raycast(
		start,
		direction,
		length,
		hit,
		visualizer);
}

void Mesh::Reload(const CreateInfo& createInfo)
{
	if (m_CreateInfo.vertices)
	{
		delete m_CreateInfo.vertices;
		m_CreateInfo.vertices = nullptr;
	}

	// TODO: Maybe need to check whether createInfo is valid!
	m_CreateInfo = createInfo;

	Buffer::CreateInfo createInfoIndexBuffer{};
	createInfoIndexBuffer.instanceSize = sizeof(m_CreateInfo.indices[0]);
	createInfoIndexBuffer.instanceCount = m_CreateInfo.indices.size();
	createInfoIndexBuffer.usages = { Buffer::Usage::INDEX_BUFFER };
	createInfoIndexBuffer.memoryType = MemoryType::GPU;
	createInfoIndexBuffer.isMultiBuffered = false;
	std::shared_ptr<Buffer> indices = Buffer::Create(createInfoIndexBuffer);

	indices->WriteToBuffer(m_CreateInfo.indices.data(), indices->GetSize());

	std::vector<std::vector<uint8_t>> vertexBuffers;
	for (size_t i = 0; i < m_CreateInfo.vertexLayouts.size(); i++)
	{
		std::vector<uint8_t>& vertexBuffer = vertexBuffers.emplace_back();
		vertexBuffer.resize(m_CreateInfo.vertexLayouts[i].size * m_CreateInfo.vertexCount);
	}

	for (size_t i = 0; i < m_CreateInfo.vertexCount; i++)
	{
		uint32_t offset = 0;
		for (size_t j = 0; j < m_CreateInfo.vertexLayouts.size(); j++)
		{
			const size_t dstOffset = i * m_CreateInfo.vertexLayouts[j].size;
			const size_t srcOffset = offset + i * m_CreateInfo.vertexSize;
			memcpy(
				vertexBuffers[j].data() + dstOffset,
				(uint8_t*)createInfo.vertices + srcOffset,
				m_CreateInfo.vertexLayouts[j].size);

			offset += m_CreateInfo.vertexLayouts[j].size;
		}
	}

	std::vector<std::shared_ptr<Buffer>> vertices;
	for (std::vector<uint8_t>& vertexBuffer : vertexBuffers)
	{
		Buffer::CreateInfo createInfoVertexBuffer{};
		createInfoVertexBuffer.instanceSize = sizeof(vertexBuffer[0]);
		createInfoVertexBuffer.instanceCount = vertexBuffer.size();
		createInfoVertexBuffer.usages = { Buffer::Usage::VERTEX_BUFFER };
		createInfoVertexBuffer.memoryType = MemoryType::GPU;
		createInfoVertexBuffer.isMultiBuffered = false;
		vertices.emplace_back(Buffer::Create(createInfoVertexBuffer));

		vertices.back()->WriteToBuffer(vertexBuffer.data(), vertexBuffer.size());
	}

	{
		std::lock_guard<std::mutex> lock(m_VertexBufferAccessMutex);
		m_Vertices = std::move(vertices);
		m_Indices = indices;
	}

	m_VertexLayoutHandles.clear();
	m_VertexLayoutHandles.resize(GetVertexLayouts().size());
	for (size_t i = 0; i < GetVertexLayouts().size(); i++)
	{
		m_VertexLayoutHandles[i] = GetVertexBuffer(i)->GetNativeHandle();
	}

	if (createInfo.boundingBox)
	{
		m_BoundingBox = *createInfo.boundingBox;
	}
	else
	{
		for (size_t i = 0; i < m_CreateInfo.vertexCount; i++)
		{
			const glm::vec3* vertexPosition = (glm::vec3*)((uint8_t*)m_CreateInfo.vertices + i * m_CreateInfo.vertexSize);

			if (vertexPosition->x < m_BoundingBox.min.x)
			{
				m_BoundingBox.min.x = vertexPosition->x;
			}

			if (vertexPosition->y < m_BoundingBox.min.y)
			{
				m_BoundingBox.min.y = vertexPosition->y;
			}

			if (vertexPosition->z < m_BoundingBox.min.z)
			{
				m_BoundingBox.min.z = vertexPosition->z;
			}

			if (vertexPosition->x > m_BoundingBox.max.x)
			{
				m_BoundingBox.max.x = vertexPosition->x;
			}

			if (vertexPosition->y > m_BoundingBox.max.y)
			{
				m_BoundingBox.max.y = vertexPosition->y;
			}

			if (vertexPosition->z > m_BoundingBox.max.z)
			{
				m_BoundingBox.max.z = vertexPosition->z;
			}
		}

		m_BoundingBox.offset = m_BoundingBox.max + (m_BoundingBox.min - m_BoundingBox.max) * 0.5f;
	}

	if (m_CreateInfo.raycastCallback)
	{
		m_CreateInfo.raycastCallback = createInfo.raycastCallback;
	}
	else
	{
		m_CreateInfo.raycastCallback = [](
			const glm::vec3& start,
			const glm::vec3& direction,
			const float length,
			std::shared_ptr<MeshBVH> bvh,
			Raycast::Hit& hit,
			Visualizer& visualizer) -> bool
			{
				//const VertexDefault* vertex = (const VertexDefault*)vertices;
				//for (size_t i = 0; i < indices.size(); i += 3)
				//{
				//	const glm::vec3& vertex0 = vertex[indices[i + 0]].position;
				//	const glm::vec3& vertex1 = vertex[indices[i + 1]].position;
				//	const glm::vec3& vertex2 = vertex[indices[i + 2]].position;

				//	const glm::vec3 a = transform * glm::vec4(vertex0, 1.0f);
				//	const glm::vec3 b = transform * glm::vec4(vertex1, 1.0f);
				//	const glm::vec3 c = transform * glm::vec4(vertex2, 1.0f);

				//	const glm::vec3 normal = glm::normalize(glm::cross((b - a), (c - a)));

				//	Raycast::Hit hit{};
				//	if (Raycast::IntersectTriangle(start, direction, a, b, c, normal, length, hit))
				//	{
				//		/*visualizer.DrawLine(a, b, { 1.0f, 0.0f, 1.0f }, 5.0f);
				//		visualizer.DrawLine(a, c, { 1.0f, 0.0f, 1.0f }, 5.0f);
				//		visualizer.DrawLine(c, b, { 1.0f, 0.0f, 1.0f }, 5.0f);*/

				//		distance = hit.distance;
				//		return true;
				//	}
				//}

				//return false;

				return bvh->Raycast(start, direction, length, hit, visualizer);
			};
	}

	m_BVH = std::make_shared<MeshBVH>(m_CreateInfo.vertices, m_CreateInfo.indices, m_CreateInfo.vertexSize);

	struct LodInfo
	{
		uint32_t indexOffset;
		uint32_t indexCount;
		float distanceThreshold;
	};

	{
		struct MeshBufferInfo
		{
			size_t vertexBufferPosition;
			size_t vertexBufferNormal;
			size_t vertexBufferColor;
			size_t vertexBufferSkinned;
			size_t indexBuffer;
		};

		Buffer::CreateInfo createInfoMeshBufferInfoBuffer{};
		createInfoMeshBufferInfoBuffer.instanceSize = sizeof(MeshBufferInfo);
		createInfoMeshBufferInfoBuffer.instanceCount = 1;
		createInfoMeshBufferInfoBuffer.usages = { Buffer::Usage::STORAGE_BUFFER };
		createInfoMeshBufferInfoBuffer.memoryType = MemoryType::GPU;
		createInfoMeshBufferInfoBuffer.isMultiBuffered = false;
		m_MeshBufferInfoBuffer = Buffer::Create(createInfoMeshBufferInfoBuffer);

		MeshBufferInfo meshBufferInfo{};

		for (size_t i = 0; i < createInfo.vertexLayouts.size(); i++)
		{
			const auto& vertexLayout = createInfo.vertexLayouts[i];
			const size_t deviceAddress = GetVertexBuffer(i)->GetDeviceAddress().Get();
			if (vertexLayout.tag == "Position")
			{
				meshBufferInfo.vertexBufferPosition = deviceAddress;
			}
			else if (vertexLayout.tag == "Normal")
			{
				meshBufferInfo.vertexBufferNormal = deviceAddress;
			}
			else if (vertexLayout.tag == "Color")
			{
				meshBufferInfo.vertexBufferColor = deviceAddress;
			}
			else if (vertexLayout.tag == "Bones")
			{
				meshBufferInfo.vertexBufferSkinned = deviceAddress;
			}

			if (deviceAddress == 0)
			{
				FATAL_ERROR("Bindless mesh is missing required vertex buffer: " + vertexLayout.tag);
			}
		}

		meshBufferInfo.indexBuffer = GetIndexBuffer()->GetDeviceAddress().Get();

		m_MeshBufferInfoBuffer->WriteToBuffer((void*)&meshBufferInfo, sizeof(MeshBufferInfo), 0);
		m_MeshBufferInfoBuffer->Flush();
	}

	{
		#define MAX_LODS 6
		struct MeshInfo
		{
			size_t meshBufferInfoBuffer;
			uint32_t lodCount;
			uint32_t flags;
			LodInfo lods[MAX_LODS];
		};

		Buffer::CreateInfo createInfoMeshInfoBuffer{};
		createInfoMeshInfoBuffer.instanceSize = sizeof(MeshInfo);
		createInfoMeshInfoBuffer.instanceCount = 1;
		createInfoMeshInfoBuffer.usages = { Buffer::Usage::STORAGE_BUFFER };
		createInfoMeshInfoBuffer.memoryType = MemoryType::GPU;
		createInfoMeshInfoBuffer.isMultiBuffered = false;
		m_MeshInfoBuffer = Buffer::Create(createInfoMeshInfoBuffer);

		MeshInfo meshInfo{};

		const auto& lods = GetLods();

		assert(lods.size() <= MAX_LODS);

		meshInfo.lodCount = static_cast<uint32_t>(lods.size());

		for (size_t i = 0; i < std::min<size_t>(lods.size(), MAX_LODS); i++)
		{
			meshInfo.lods[i].indexOffset = static_cast<uint32_t>(lods[i].indexOffset);
			meshInfo.lods[i].indexCount = static_cast<uint32_t>(lods[i].indexCount);
			meshInfo.lods[i].distanceThreshold = lods[i].distanceThreshold;
		}

		meshInfo.meshBufferInfoBuffer = m_MeshBufferInfoBuffer->GetDeviceAddress().Get();

		m_MeshInfoBuffer->WriteToBuffer((void*)&meshInfo, sizeof(MeshInfo), 0);
		m_MeshInfoBuffer->Flush();
	}
}
