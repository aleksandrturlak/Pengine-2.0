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
	nodes.reserve(r3dView.size() * 2);

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

		AABB aabb = Utils::LocalToWorldAABB({ r3d.mesh->GetBoundingBox().min, r3d.mesh->GetBoundingBox().max }, transform.GetTransform());
		const float distance2 = glm::distance2(aabb.max, aabb.min);
		if (distance2 < 1e-6f)
		{
			continue;
		}

		BVHNode node{};
		node.aabb = std::move(aabb);
		node.entity = transform.GetEntity();
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

	std::stack<uint32_t> nodeStack;
	nodeStack.push(m_Root);

	while (!nodeStack.empty())
	{
		const BVHNode& currentNode = m_Nodes[nodeStack.top()];
		nodeStack.pop();

		if (callback(currentNode))
		{
			if (currentNode.right != -1) nodeStack.push(currentNode.right);
			if (currentNode.left != -1) nodeStack.push(currentNode.left);
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
		
		if (node.IsLeaf() && node.entity->IsValid())
		{
			visibleEntities.emplace_back(node.entity->GetHandle());
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

		if (node.IsLeaf() && node.entity->IsValid())
		{
			visibleEntities.emplace_back(node.entity->GetHandle());
		}

		return true;
	});

	return visibleEntities;
}

std::multimap<Raycast::Hit, std::shared_ptr<Entity>> SceneBVH::Raycast(
	const glm::vec3& start,
	const glm::vec3& direction,
	const float length) const
{
	PROFILER_SCOPE(__FUNCTION__);

	std::multimap<Raycast::Hit, std::shared_ptr<Entity>> hits;

	Traverse([start, direction, length, &hits](const BVHNode& node)
	{
		Raycast::Hit hit{};
		if (!Raycast::IntersectBoxAABB(start, direction, node.aabb.min, node.aabb.max, length, hit))
		{
			return false;
		}

		if (node.IsLeaf() && node.entity->IsValid())
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
	m_Nodes.clear();

	m_Nodes = std::move(nodes);

	std::atomic<int> parallel = 2;
	m_Root = BuildRecursive(0, m_Nodes.size(), parallel);
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

uint32_t SceneBVH::BuildRecursive(int start, int end, std::atomic<int>& parallel)
{
	const int count = end - start;
	if (count == 0) return -1;

	if (count == 1)
	{
		return start;
	}

	AABB centroidBounds = m_Nodes[start].aabb;
	for (int i = start + 1; i < end; i++)
	{
		centroidBounds = centroidBounds.Expanded(m_Nodes[i].aabb);
	}

	// Choose split axis (longest).
	int axis = 0;
	glm::vec3 extent =
	{
		centroidBounds.max.x - centroidBounds.min.x,
		centroidBounds.max.y - centroidBounds.min.y,
		centroidBounds.max.z - centroidBounds.min.z
	};
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;

	// Partition using SAH.
	const int BIN_COUNT = 12;
	struct Bin
	{
		AABB bounds;
		int count = 0;
	} bins[BIN_COUNT];

	// Single pass: Fill bins with counts and bounds.
	float scale = BIN_COUNT / extent[axis];
	for (int i = start; i < end; i++)
	{
		int binIdx = std::min(BIN_COUNT - 1,
			static_cast<int>((m_Nodes[i].aabb.Center()[axis] - centroidBounds.min[axis]) * scale));
		bins[binIdx].count++;
		bins[binIdx].bounds = bins[binIdx].bounds.Expanded(m_Nodes[i].aabb);
	}

	// Build prefix arrays: Left -> Right and Right -> Left
	AABB leftBounds[BIN_COUNT], rightBounds[BIN_COUNT];
	int leftCount[BIN_COUNT], rightCount[BIN_COUNT];

	AABB currentLeft, currentRight;
	int currentLeftCount = 0, currentRightCount = 0;
	for (int i = 0; i < BIN_COUNT; i++)
	{
		if (bins[i].count > 0)
		{
			currentLeft = currentLeft.Expanded(bins[i].bounds);
			currentLeftCount += bins[i].count;
		}
		leftBounds[i] = currentLeft;
		leftCount[i] = currentLeftCount;

		int j = BIN_COUNT - 1 - i;
		if (bins[j].count > 0)
		{
			currentRight = currentRight.Expanded(bins[j].bounds);
			currentRightCount += bins[j].count;
		}
		rightBounds[j] = currentRight;
		rightCount[j] = currentRightCount;
	}

	// Evaluate splits in O(BIN_COUNT)
	float bestCost = std::numeric_limits<float>::infinity();
	int bestSplit = -1;
	for (int i = 1; i < BIN_COUNT; i++)
	{
		if (leftCount[i - 1] == 0 || rightCount[i] == 0) continue;

		float cost = leftCount[i - 1] * leftBounds[i - 1].SurfaceArea() +
			rightCount[i] * rightBounds[i].SurfaceArea();
		if (cost < bestCost)
		{
			bestCost = cost;
			bestSplit = i;
		}
	}

	int mid = Partition(BIN_COUNT, start, end, axis, scale, centroidBounds.min[axis], bestSplit);

	// Handle bad splits.
	if (mid == start || mid == end)
	{
		mid = start + count / 2;
	}

	std::vector<std::future<void>> futures;

	// Recursively build children.
	BVHNode node{};
	if (parallel.fetch_sub(1) > 0)
	{
		futures.emplace_back(m_ThreadPool.EnqueueAsyncFuture([&]()
		{
			node.left = BuildRecursive(start, mid, parallel);
		}));
	}
	else
	{
		node.left = BuildRecursive(start, mid, parallel);
	}

	if (parallel.fetch_sub(1) > 0)
	{
		futures.emplace_back(m_ThreadPool.EnqueueAsyncFuture([&]()
		{
			node.right = BuildRecursive(mid, end, parallel);
		}));
	}
	else
	{
		node.right = BuildRecursive(mid, end, parallel);
	}
	
	for (auto& future : futures)
	{
		future.get();
	}

	const BVHNode& left = m_Nodes[node.left];
	const BVHNode& right = m_Nodes[node.right];
	node.aabb = left.aabb.Expanded(right.aabb);
	node.subtreeSize = left.subtreeSize + right.subtreeSize;

	std::lock_guard<std::mutex> lock(m_LockWrite);
	m_Nodes.emplace_back(std::move(node));

	return m_Nodes.size() - 1;
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
