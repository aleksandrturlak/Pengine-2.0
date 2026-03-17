#pragma once

#include "Core.h"
#include "Raycast.h"
#include "BoundingBox.h"
#include "ThreadPool.h"

namespace Pengine
{
	class Scene;
	class Entity;

	class PENGINE_API SceneBVH
	{
	public:

		struct BVHNode
		{
			AABB aabb;
			uint32_t left = -1;
			uint32_t right = -1;
			entt::entity entity;
			int subtreeSize = 0;

			[[nodiscard]] bool IsLeaf() const { return left == -1 && right == -1; }
		};

		SceneBVH();
		~SceneBVH();

		void Clear();

		static std::vector<BVHNode> BuildNodes(const entt::registry& registry);

		void Update(std::vector<BVHNode>&& nodes);

		void Traverse(const std::function<bool(const BVHNode&)>& callback) const;

		std::vector<entt::entity> CullAgainstFrustum(const std::array<glm::vec4, 6>& planes);

		std::vector<entt::entity> CullAgainstSphere(const glm::vec3& position, float radius);

		std::multimap<Pengine::Raycast::Hit, entt::entity> Raycast(
			const glm::vec3& start,
			const glm::vec3& direction,
			const float length) const;

		[[nodiscard]] std::optional<BVHNode> GetRoot() const { return m_Root == -1 ? std::nullopt : std::optional<BVHNode>(m_Nodes[m_Root]); }

	private:

		uint32_t m_Root = -1;
		std::vector<BVHNode> m_Nodes;

		/**
		 * Protection that there is currently no use (ray cast or traverse in progress) before rebuilding or clearing.
		 */
		std::mutex m_LockWrite;
		std::mutex m_LockBVH;
		mutable std::condition_variable m_BVHConditionalVariable;
		mutable std::atomic<size_t> m_BVHUseCount;

		ThreadPool m_ThreadPool;

		void WaitIdle();

		void Rebuild(std::vector<BVHNode>&& nodes);

		int Partition(const int binCount, int start, int end, int axis, float scale, float minAxis, int bestSplit);

		uint32_t BuildRecursive(int start, int end, std::atomic<uint32_t>& nextInterior, std::atomic<int>& parallelBudget);

		//BVHNode* FindLeaf(BVHNode* node, std::shared_ptr<Entity> entity) const;

		//BVHNode* FindParent(BVHNode* root, BVHNode* target) const;
	};

}
