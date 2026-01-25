#pragma once

#include "../Core/Core.h"
#include "../Core/Asset.h"
#include "../Core/BoundingBox.h"
#include "../Core/Visualizer.h"

#include "Buffer.h"
#include "Vertex.h"
#include "MeshBVH.h"

namespace Pengine
{

	class PENGINE_API Mesh final : public Asset
	{
	public:
		enum class Type
		{
			STATIC,
			SKINNED
		};

		struct Lod
		{
			size_t indexCount = 0;
			size_t indexOffset = 0;
			float distanceThreshold = 0.0f;
		};

		struct CreateInfo
		{
			struct SourceFileInfo
			{
				std::filesystem::path filepath;
				std::string meshName;
				uint32_t primitiveIndex;
			};

			SourceFileInfo sourceFileInfo;
			std::string name;
			std::filesystem::path filepath;
			std::vector<VertexLayout> vertexLayouts;
			std::vector<Lod> lods;
			size_t vertexCount = 0;
			uint32_t vertexSize = 0;
			void* vertices = nullptr;
			std::vector<uint32_t> indices;
			std::optional<BoundingBox> boundingBox;
			Type type = Type::STATIC;

			std::function<bool(
				const glm::vec3& start,
				const glm::vec3& direction,
				const float length,
				std::shared_ptr<MeshBVH> bvh,
				Raycast::Hit& hit,
				Visualizer& visualizer)> raycastCallback;
		};

		Mesh(const CreateInfo& createInfo);
		Mesh(const Mesh&) = delete;
		Mesh(Mesh&&) = delete;
		~Mesh();
		Mesh& operator=(const Mesh&) = delete;
		Mesh& operator=(Mesh&&) = delete;

		[[nodiscard]] const std::shared_ptr<Buffer>& GetVertexBuffer(const size_t index) const;

		[[nodiscard]] const std::shared_ptr<Buffer>& GetIndexBuffer() const { return m_Indices; };

		[[nodiscard]] const std::shared_ptr<Buffer>& GetMeshInfoBuffer() const;

		[[nodiscard]] const void* GetRawVertices() const { return m_CreateInfo.vertices; }

		[[nodiscard]] const std::vector<uint32_t>& GetRawIndices() const { return m_CreateInfo.indices; };

		[[nodiscard]] size_t GetVertexCount() const { return m_CreateInfo.vertexCount; }

		[[nodiscard]] size_t GetIndexCount() const { return m_CreateInfo.indices.size(); }

		[[nodiscard]] uint32_t GetVertexSize() const { return m_CreateInfo.vertexSize; }

		[[nodiscard]] const BoundingBox& GetBoundingBox() const { return m_BoundingBox; }

		void SetBoundingBox(BoundingBox& boundingBox) { m_BoundingBox = boundingBox; }

		[[nodiscard]] const std::vector<VertexLayout>& GetVertexLayouts() const { return m_CreateInfo.vertexLayouts; }

		[[nodiscard]] const std::vector<NativeHandle>& GetVertexLayoutHandles() const { return m_VertexLayoutHandles; }

		[[nodiscard]] Type GetType() const { return m_CreateInfo.type; }

		[[nodiscard]] const CreateInfo GetCreateInfo() const { return m_CreateInfo; }

		[[nodiscard]] std::shared_ptr<MeshBVH> GetBVH() const { return m_BVH; }

		[[nodiscard]] const std::vector<Lod>& GetLods() const { return m_CreateInfo.lods; }

		[[nodiscard]] bool Raycast(
			const glm::vec3& start,
			const glm::vec3& direction,
			const float length,
			Raycast::Hit& hit,
			Visualizer& visualizer) const;

		[[nodiscard]] bool IsBindless() const { return m_BindlessIndex != -1; }

		[[nodiscard]] int GetBindlessIndex() const { return m_BindlessIndex; }

		void SetBindlessIndex(const int index) { m_BindlessIndex = index; }

		void Reload(const CreateInfo& createInfo);

	private:
		std::shared_ptr<MeshBVH> m_BVH;
		std::vector<std::shared_ptr<Buffer>> m_Vertices;
		std::vector<NativeHandle> m_VertexLayoutHandles;
		std::shared_ptr<Buffer> m_Indices;
		std::shared_ptr<Buffer> m_MeshInfoBuffer;
		std::shared_ptr<Buffer> m_MeshBufferInfoBuffer;
		BoundingBox m_BoundingBox{};
		CreateInfo m_CreateInfo{};

		int m_BindlessIndex = -1;
		
		mutable std::mutex m_VertexBufferAccessMutex;
	};

}