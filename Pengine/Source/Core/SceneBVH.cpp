#include "SceneBVH.h"

#include "Scene.h"
#include "Profiler.h"

#include "../Components/Transform.h"
#include "../Components/Renderer3D.h"

#include "../Graphics/Mesh.h"

#include "../Utils/Utils.h"

using namespace Pengine;

Pengine::SceneBVH::SceneBVH()
{
	m_ThreadPool.Initialize(2);
}

Pengine::SceneBVH::~SceneBVH()
{
	Clear();
	m_ThreadPool.Shutdown();
}

void SceneBVH::Clear()
{
	PROFILER_SCOPE(__FUNCTION__);

	WaitIdle();

	m_Nodes.clear();
	m_Root = -1;
}

std::vector<SceneBVH::BVHNode> SceneBVH::BuildNodes(const entt::registry& registry)
{
	PROFILER_SCOPE(__FUNCTION__);

	std::vector<SceneBVH::BVHNode> nodes;

	const auto r3dView = registry.view<Renderer3D>();
	nodes.reserve(r3dView.size());
	
	for (auto entity : r3dView)
	{
		const Transform& transform = registry.get<Transform>(entity);
		if (!transform.GetEntity()->IsEnabled())
		{
			continue;
		}

		const Renderer3D& r3d = registry.get<Renderer3D>(entity);
		if (!r3d.mesh || !r3d.isEnabled)
		{
			continue;
		}

		const glm::vec3 diag = r3d.aabb.max - r3d.aabb.min;
		if (diag.x * diag.x + diag.y * diag.y + diag.z * diag.z < 1e-6f)
		{
			continue;
		}

		BVHNode node{};
		node.aabb = r3d.aabb;
		node.entity = entity;
		node.subtreeSize = 1;
		nodes.emplace_back(std::move(node));
	}

	return nodes;
}

void SceneBVH::Update(std::vector<BVHNode>&& nodes)
{
	PROFILER_SCOPE(__FUNCTION__);

	WaitIdle();

	if (nodes.empty())
	{
		m_Root = -1;
		m_Nodes.clear();
		return;
	}

	Rebuild(std::move(nodes));
}

void SceneBVH::Traverse(const std::function<bool(const BVHNode&)>& callback) const
{
	PROFILER_SCOPE(__FUNCTION__);

	if (m_Root == -1) return;

	m_BVHUseCount.fetch_add(1);

	uint32_t stack[64];
	int top = 0;
	stack[top++] = m_Root;

	while (top > 0)
	{
		const BVHNode& currentNode = m_Nodes[stack[--top]];

		if (callback(currentNode))
		{
			if (currentNode.right != static_cast<uint32_t>(-1)) stack[top++] = currentNode.right;
			if (currentNode.left  != static_cast<uint32_t>(-1)) stack[top++] = currentNode.left;
		}
	}

	m_BVHUseCount.fetch_sub(1);
	m_BVHConditionalVariable.notify_all();
}

std::vector<entt::entity> SceneBVH::CullAgainstFrustum(const std::array<glm::vec4, 6>& planes)
{
	PROFILER_SCOPE(__FUNCTION__);

	std::vector<entt::entity> visibleEntities;
	Traverse([&planes, &visibleEntities](const BVHNode& node)
	{
		if (!Utils::isAABBInsideFrustum(planes, node.aabb.min, node.aabb.max))
		{
			return false;
		}
		
		if (node.IsLeaf())
		{
			visibleEntities.emplace_back(node.entity);
		}

		return true;
	});

	return visibleEntities;
}

std::vector<entt::entity> SceneBVH::CullAgainstSphere(const glm::vec3& position, float radius)
{
	PROFILER_SCOPE(__FUNCTION__);

	std::vector<entt::entity> visibleEntities;
	Traverse([&position, radius, &visibleEntities](const BVHNode& node)
	{
		if (!Utils::IntersectAABBvsSphere(node.aabb.min, node.aabb.max, position, radius))
		{
			return false;
		}

		if (node.IsLeaf())
		{
			visibleEntities.emplace_back(node.entity);
		}

		return true;
	});

	return visibleEntities;
}

std::multimap<Raycast::Hit, entt::entity> SceneBVH::Raycast(
	const glm::vec3& start,
	const glm::vec3& direction,
	const float length) const
{
	PROFILER_SCOPE(__FUNCTION__);

	std::multimap<Raycast::Hit, entt::entity> hits;

	Traverse([start, direction, length, &hits](const BVHNode& node)
	{
		Raycast::Hit hit{};
		if (!Raycast::IntersectBoxAABB(start, direction, node.aabb.min, node.aabb.max, length, hit))
		{
			return false;
		}

		if (node.IsLeaf())
		{
			hits.emplace(hit, node.entity);
		}

		return true;
	});

	return hits;
}

void SceneBVH::WaitIdle()
{
	std::unique_lock<std::mutex> lock(m_LockBVH);
	m_BVHConditionalVariable.wait(lock, [this]
	{
		return m_BVHUseCount.load() == 0;
	});
}

void SceneBVH::Rebuild(std::vector<BVHNode>&& nodes)
{
	PROFILER_SCOPE(__FUNCTION__);

	m_Root = -1;

	const uint32_t leafCount = static_cast<uint32_t>(nodes.size());
	const uint32_t totalCount = leafCount * 2 - 1;

	m_Nodes.reserve(totalCount);
	m_Nodes.resize(leafCount);
	for (uint32_t i = 0; i < leafCount; i++)
	{
		m_Nodes[i] = std::move(nodes[i]);
	}
	m_Nodes.resize(totalCount);

	std::atomic<uint32_t> nextInterior = leafCount;
	std::atomic<int> parallelBudget = 2;
	m_Root = BuildRecursive(0, static_cast<int>(leafCount), nextInterior, parallelBudget);
}

int SceneBVH::Partition(const int binCount, int start, int end, int axis, float scale, float minAxis, int bestSplit)
{
	int i = start, j = end - 1;
	while (i <= j)
	{
		// Find node that belongs to right side.
		while (i <= j)
		{
			int binIdx = std::min(binCount - 1,
				static_cast<int>((m_Nodes[i].aabb.Center()[axis] - minAxis) * scale));
			if (binIdx >= bestSplit) break;
			i++;
		}
		// Find node that belongs to left side.
		while (i <= j)
		{
			int binIdx = std::min(binCount - 1,
				static_cast<int>((m_Nodes[j].aabb.Center()[axis] - minAxis) * scale));
			if (binIdx < bestSplit) break;
			j--;
		}
		// Swap if found mismatched pair.
		if (i < j)
		{
			std::swap(m_Nodes[i], m_Nodes[j]);
			i++; j--;
		}
	}
	return i;
}

uint32_t SceneBVH::BuildRecursive(int start, int end, std::atomic<uint32_t>& nextInterior, std::atomic<int>& parallelBudget)
{
	const int count = end - start;
	if (count == 0) return -1;

	if (count == 1)
	{
		return start;
	}

	// Compute bounds of centroids (not AABBs) — correct SAH input.
	// Also cache the centroid of each node to avoid recomputing it in binning and partition.
	glm::vec3 centMin = m_Nodes[start].aabb.Center();
	glm::vec3 centMax = centMin;
	for (int i = start + 1; i < end; i++)
	{
		const glm::vec3 c = m_Nodes[i].aabb.Center();
		centMin = glm::min(centMin, c);
		centMax = glm::max(centMax, c);
	}

	const glm::vec3 extent = centMax - centMin;

	// Choose split axis (longest centroid extent).
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;

	// All centroids coincide on this axis — fall back to median.
	if (extent[axis] < 1e-6f)
	{
		const uint32_t nodeIdx = nextInterior.fetch_add(1);
		const int mid = start + count / 2;
		const uint32_t leftIdx  = BuildRecursive(start, mid, nextInterior, parallelBudget);
		const uint32_t rightIdx = BuildRecursive(mid,   end, nextInterior, parallelBudget);
		BVHNode& node = m_Nodes[nodeIdx];
		node.left  = leftIdx;
		node.right = rightIdx;
		node.aabb  = m_Nodes[leftIdx].aabb.Expanded(m_Nodes[rightIdx].aabb);
		node.subtreeSize = m_Nodes[leftIdx].subtreeSize + m_Nodes[rightIdx].subtreeSize;
		return nodeIdx;
	}

	// SAH binning.
	const int BIN_COUNT = 12;
	struct Bin { AABB bounds; int count = 0; } bins[BIN_COUNT];

	const float scale = BIN_COUNT / extent[axis];
	const float axisMin = centMin[axis];

	for (int i = start; i < end; i++)
	{
		const float c = (m_Nodes[i].aabb.min[axis] + m_Nodes[i].aabb.max[axis]) * 0.5f;
		const int binIdx = std::min(BIN_COUNT - 1, static_cast<int>((c - axisMin) * scale));
		bins[binIdx].count++;
		bins[binIdx].bounds = bins[binIdx].bounds.Expanded(m_Nodes[i].aabb);
	}

	// Prefix sweep: left->right and right->left.
	AABB leftBounds[BIN_COUNT], rightBounds[BIN_COUNT];
	int  leftCount[BIN_COUNT],  rightCount[BIN_COUNT];

	AABB curL, curR;
	int  cntL = 0, cntR = 0;
	for (int i = 0; i < BIN_COUNT; i++)
	{
		if (bins[i].count > 0) { curL = curL.Expanded(bins[i].bounds); cntL += bins[i].count; }
		leftBounds[i] = curL;
		leftCount[i]  = cntL;

		const int j = BIN_COUNT - 1 - i;
		if (bins[j].count > 0) { curR = curR.Expanded(bins[j].bounds); cntR += bins[j].count; }
		rightBounds[j] = curR;
		rightCount[j]  = cntR;
	}

	// Pick best split.
	float bestCost = std::numeric_limits<float>::infinity();
	int bestSplit = 1;
	for (int i = 1; i < BIN_COUNT; i++)
	{
		if (leftCount[i - 1] == 0 || rightCount[i] == 0) continue;
		const float cost = leftCount[i - 1] * leftBounds[i - 1].SurfaceArea()
		                 + rightCount[i]     * rightBounds[i].SurfaceArea();
		if (cost < bestCost) { bestCost = cost; bestSplit = i; }
	}

	int mid = Partition(BIN_COUNT, start, end, axis, scale, axisMin, bestSplit);
	if (mid == start || mid == end)
	{
		mid = start + count / 2;
	}

	// Claim a pre-allocated interior node slot atomically.
	const uint32_t nodeIdx = nextInterior.fetch_add(1);

	uint32_t leftIdx = -1, rightIdx = -1;

	if (parallelBudget.fetch_sub(1) > 0)
	{
		auto future = m_ThreadPool.EnqueueAsyncFuture([&]()
		{
			leftIdx = BuildRecursive(start, mid, nextInterior, parallelBudget);
		});
		rightIdx = BuildRecursive(mid, end, nextInterior, parallelBudget);
		future.get();
	}
	else
	{
		leftIdx  = BuildRecursive(start, mid, nextInterior, parallelBudget);
		rightIdx = BuildRecursive(mid,   end, nextInterior, parallelBudget);
	}

	BVHNode& node = m_Nodes[nodeIdx];
	node.left  = leftIdx;
	node.right = rightIdx;
	node.aabb  = m_Nodes[leftIdx].aabb.Expanded(m_Nodes[rightIdx].aabb);
	node.subtreeSize = m_Nodes[leftIdx].subtreeSize + m_Nodes[rightIdx].subtreeSize;

	return nodeIdx;
}

//SceneBVH::BVHNode* SceneBVH::FindLeaf(BVHNode* node, std::shared_ptr<Entity> entity) const
//{
//	if (!node) return nullptr;
//	if (node->entity == entity) return node;
//
//	if (BVHNode* left = FindLeaf(node->left, entity)) return left;
//	return FindLeaf(node->right, entity);
//}
//
//SceneBVH::BVHNode* SceneBVH::FindParent(BVHNode* root, BVHNode* target) const
//{
//	if (!root || root == target) return nullptr;
//	if (root->left == target || root->right == target) return root;
//
//	if (BVHNode* left = FindParent(root->left, target)) return left;
//	return FindParent(root->right, target);
//}
