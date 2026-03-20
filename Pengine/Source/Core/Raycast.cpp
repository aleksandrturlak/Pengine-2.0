#include "Raycast.h"

#include "SceneManager.h"
#include "Profiler.h"
#include "Scene.h"

#include "../Graphics/Mesh.h"
#include "../Graphics/Vertex.h"
#include "../Components/Renderer3D.h"
#include "../Components/Transform.h"
#include "../ComponentSystems/PhysicsSystem.h"

#include "../Utils/Utils.h"

#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>

#include "../Components/RigidBody.h"

using namespace Pengine;

bool Raycast::IntersectTriangle(
	const glm::vec3& start,
	const glm::vec3& direction,
	const glm::vec3& a,
	const glm::vec3& b,
	const glm::vec3& c,
	const glm::vec3& planeNormal,
	const float length,
	Hit& hit)
{
	const glm::vec3 distance = direction * length;
	const glm::vec3 distanceToPlane = a - start;
	const float vp = glm::dot(distance, planeNormal);
	if (vp >= -glm::epsilon<float>() && vp <= glm::epsilon<float>())
	{
		return false;
	}

	const float wp = glm::dot(distanceToPlane, planeNormal);
	const float t = wp / vp;
	const glm::vec3 point = distance * t + start;
	hit.normal = planeNormal;
	hit.point = point;
	hit.distance = glm::distance(start, point);
	const glm::vec3 edge0 = b - a;
	const glm::vec3 edge1 = c - b;
	const glm::vec3 edge2 = a - c;
	const glm::vec3 c0 = point - a;
	const glm::vec3 c1 = point - b;
	const glm::vec3 c2 = point - c;

	if (glm::dot(glm::normalize(point - start), direction) < 0.0f)
	{
		return false;
	}

	if (glm::dot(planeNormal, glm::cross(edge0, c0)) > 0.0f &&
		glm::dot(planeNormal, glm::cross(edge1, c1)) > 0.0f &&
		glm::dot(planeNormal, glm::cross(edge2, c2)) > 0.0f)
	{
		return true;
	}

	return false;
}

bool Raycast::IntersectBoxOBB(
	const glm::vec3& start,
	const glm::vec3& direction,
	const glm::vec3& min,
	const glm::vec3& max,
	const glm::vec3& position,
	const glm::vec3& scale,
	const glm::mat4& rotation,
	const float length,
	Hit& hit)
{
	glm::vec3 obbMin = min * scale;
	glm::vec3 obbMax = max * scale;
	const float* transformPtr = glm::value_ptr(rotation);
	glm::vec3 axis{};
	glm::vec3 distance = position - start;

	float tMin = 0.0f, tMax = std::numeric_limits<float>().max(), nomLen, denomLen, axesMin, axesMax;
	size_t row = 0;

	for (size_t i = 0; i < 3; i++)
	{
		row = i * 4;
		axis = glm::vec3(transformPtr[row], transformPtr[row + 1], transformPtr[row + 2]);
		nomLen = glm::dot(axis, distance);
		denomLen = glm::dot(direction, axis);
		axis = glm::normalize(axis);
		if (glm::abs(denomLen) > glm::epsilon<float>())
		{
			axesMin = (nomLen + obbMin[i]) / denomLen;
			axesMax = (nomLen + obbMax[i]) / denomLen;

			if (axesMin > axesMax) { std::swap(axesMin, axesMax); }
			if (axesMin > tMin) tMin = axesMin;
			if (axesMax < tMax) tMax = axesMax;

			if (tMax < tMin) return false;
		}
		else if (-nomLen + obbMin[i] > 0.0f || -nomLen + obbMax[i] < 0.0f) return false;
	}

	hit.distance = tMin;
	hit.point = tMin * direction + start;

	return hit.distance < length;
}

bool Raycast::IntersectBoxAABB(
	const glm::vec3& start,
	const glm::vec3& direction,
	const glm::vec3& min,
	const glm::vec3& max,
	const float length,
	Hit& hit)
{
	float tMin = 0.001f;
	float tMax = length;

	for (int i = 0; i < 3; i++)
	{
		float invDir = 1.0f / direction[i];
		float t1 = (min[i] - start[i]) * invDir;
		float t2 = (max[i] - start[i]) * invDir;

		if (invDir < 0.0f) std::swap(t1, t2);

		tMin = t1 > tMin ? t1 : tMin;
		tMax = t2 < tMax ? t2 : tMax;

		if (tMax < tMin) return false;
	}

	hit.distance = (tMin <= tMax) ? tMin : std::numeric_limits<float>::infinity();
	hit.point = start + direction * hit.distance;

	return true;
}

std::map<Raycast::Hit, std::shared_ptr<Entity>> Raycast::RaycastScene(
	std::shared_ptr<Scene> scene,
	const glm::vec3& start,
	const glm::vec3& direction,
	const float length)
{
	PROFILER_SCOPE(__FUNCTION__);

	const auto sceneBvhHits = scene->GetBVH()->Raycast(start, direction, length);

	std::map<Hit, std::shared_ptr<Entity>> hits;

	for (const auto& [hit, entity] : sceneBvhHits)
	{
		if (!scene->GetRegistry().valid(entity)) continue;
		const Transform& transform = scene->GetRegistry().get<Transform>(entity);
		const Renderer3D& r3d = scene->GetRegistry().get<Renderer3D>(entity);

		Hit localHitMesh{};

		const glm::mat4 inverseTransformMat4 = transform.GetInverseTransformMat4();
		const glm::vec4 localStartHomogeneous = inverseTransformMat4 * glm::vec4(start, 1.0f);
		const glm::vec3 localStart = glm::vec3(localStartHomogeneous) / localStartHomogeneous.w;

		const glm::vec4 localEndHomogeneous = inverseTransformMat4 * glm::vec4(start + direction, 1.0f);
		const glm::vec3 localEnd = glm::vec3(localEndHomogeneous) / localEndHomogeneous.w;

		const glm::vec3 localDirection = glm::normalize(localEnd - localStart);

		if (r3d.mesh->Raycast(localStart, localDirection, length, localHitMesh, scene->GetVisualizer()))
		{
			Hit worldHitMesh{};
			worldHitMesh.uv = localHitMesh.uv;
			worldHitMesh.point = transform.GetTransform() * glm::vec4(localHitMesh.point, 1.0f);
			worldHitMesh.normal = glm::normalize(transform.GetInverseTransform() * glm::vec4(localHitMesh.normal, 0.0f));
			worldHitMesh.distance = glm::distance(start, worldHitMesh.point);

			hits.emplace(worldHitMesh, transform.GetEntity());
		}
	}

	/*if (!hits.empty())
	{
		scene->GetVisualizer().DrawSphere(hits.begin()->first.point, 0.1f, 8, {1.0f, 1.0f, 1.0f});
		scene->GetVisualizer().DrawLine(hits.begin()->first.point, hits.begin()->first.point + hits.begin()->first.normal, { 1.0f, 0.0f, 1.0f });
	}*/

	return hits;
}

bool Raycast::RaycastEntity(
	std::shared_ptr<Entity> entity,
	const glm::vec3& start,
	const glm::vec3& direction,
	const float length,
	Hit& hit)
{
	if (!entity || !entity->IsEnabled())
	{
		return false;
	}

	const std::shared_ptr<Scene> scene = entity->GetScene();
	const Renderer3D& r3d = scene->GetRegistry().get<Renderer3D>(entity->GetHandle());
	const Transform& transform = scene->GetRegistry().get<Transform>(entity->GetHandle());
	if (!r3d.isEnabled || !r3d.mesh)
	{
		return false;
	}

	Hit hitOBB{};
	if (IntersectBoxOBB(
		start,
		direction,
		r3d.mesh->GetBoundingBox().min,
		r3d.mesh->GetBoundingBox().max,
		transform.GetPosition(),
		transform.GetScale(),
		transform.GetRotationMat4(),
		length,
		hitOBB))
	{
		const auto hits = RaycastScene(scene, start, direction, length);
		if (!hits.empty() && hits.begin()->second == entity)
		{
			hit = hits.begin()->first;
			return true;
		}
	}

	return false;
}

bool Raycast::RaycastPhysics(
	std::shared_ptr<Scene> scene,
	const glm::vec3& origin,
	const glm::vec3& direction,
	float length,
	PhysicsHit& hit,
	std::shared_ptr<Entity> ignoredEntity)
{
	if (!scene)
	{
		return false;
	}

	const JPH::Vec3 joltOrigin = GlmVec3ToJoltVec3(origin);
	const JPH::Vec3 joltDirection = GlmVec3ToJoltVec3(direction * length);

	JPH::RRayCast ray(joltOrigin, joltDirection);
	JPH::RayCastResult result;

	struct IgnoreBodyFilter : public JPH::BodyFilter
	{
		JPH::BodyID m_ignored;
		explicit IgnoreBodyFilter(JPH::BodyID id) : m_ignored(id) {}
		bool ShouldCollide(const JPH::BodyID& id) const override { return id != m_ignored; }
	};

	bool didHit;
	if (ignoredEntity && ignoredEntity->HasComponent<RigidBody>())
	{
		const RigidBody& rb = ignoredEntity->GetComponent<RigidBody>();
		if (rb.isValid)
		{
			IgnoreBodyFilter filter(rb.id);
			didHit = scene->GetPhysicsSystem()->GetInstance().GetNarrowPhaseQuery().CastRay(ray, result, {}, {}, filter);
		}
		else
		{
			didHit = scene->GetPhysicsSystem()->GetInstance().GetNarrowPhaseQuery().CastRay(ray, result);
		}
	}
	else
	{
		didHit = scene->GetPhysicsSystem()->GetInstance().GetNarrowPhaseQuery().CastRay(ray, result);
	}

	if (!didHit)
	{
		return false;
	}

	const entt::entity handle = scene->GetPhysicsSystem()->GetEntity(result.mBodyID);
	if (handle == entt::tombstone)
	{
		return false;
	}

	hit.fraction = result.mFraction;
	hit.point    = origin + direction * (length * result.mFraction);
	hit.entity   = scene->GetRegistry().get<Transform>(handle).GetEntity();

	// Fetch the surface normal from Jolt
	{
		JPH::BodyLockRead lock(scene->GetPhysicsSystem()->GetInstance().GetBodyLockInterface(), result.mBodyID);
		if (lock.Succeeded())
		{
			const JPH::Vec3 joltHitPoint = GlmVec3ToJoltVec3(hit.point);
			const JPH::Vec3 joltNormal   = lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, joltHitPoint);
			hit.normal = JoltVec3ToGlmVec3(joltNormal);
		}
	}

	return true;
}

bool Raycast::ShapecastSpherePhysics(
	std::shared_ptr<Scene> scene,
	const glm::vec3& origin,
	const glm::vec3& direction,
	float radius,
	float length,
	PhysicsHit& hit)
{
	if (!scene)
	{
		return false;
	}

	const JPH::SphereShape sphere(radius);
	const JPH::RVec3 joltOrigin = GlmVec3ToJoltVec3(origin);
	const JPH::Vec3 joltDirection = GlmVec3ToJoltVec3(direction * length);
	const JPH::RShapeCast shapeCast = JPH::RShapeCast::sFromWorldTransform(
		&sphere,
		JPH::Vec3::sReplicate(1.0f),
		JPH::RMat44::sTranslation(joltOrigin),
		joltDirection);

	JPH::ShapeCastSettings settings;
	JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
	scene->GetPhysicsSystem()->GetInstance().GetNarrowPhaseQuery().CastShape(
		shapeCast, settings, JPH::RVec3::sZero(), collector);

	if (!collector.HadHit())
	{
		return false;
	}

	const entt::entity handle = scene->GetPhysicsSystem()->GetEntity(collector.mHit.mBodyID2);
	if (handle == entt::tombstone)
	{
		return false;
	}

	hit.fraction = collector.mHit.mFraction;
	hit.point = origin + direction * (length * collector.mHit.mFraction);
	hit.entity = scene->GetRegistry().get<Transform>(handle).GetEntity();
	return true;
}

Raycast::OutCode Raycast::ComputeOutCode(
	const glm::vec2& point,
	const glm::vec2& min,
	const glm::vec2& max)
{
	constexpr int INSIDE = 0b0000;
	constexpr int LEFT = 0b0001;
	constexpr int RIGHT = 0b0010;
	constexpr int BOTTOM = 0b0100;
	constexpr int TOP = 0b1000;

	OutCode code = INSIDE;

	if (point.x < min.x)
		code |= LEFT;
	else if (point.x > max.x)
		code |= RIGHT;
	if (point.y < min.y)
		code |= BOTTOM;
	else if (point.y > max.y)
		code |= TOP;

	return code;
}

bool Raycast::CohenSutherlandLineClip(
	glm::vec2 start,
	glm::vec2 end,
	const glm::vec2& min,
	const glm::vec2& max)
{
	constexpr int INSIDE = 0b0000;
	constexpr int LEFT = 0b0001;
	constexpr int RIGHT = 0b0010;
	constexpr int BOTTOM = 0b0100;
	constexpr int TOP = 0b1000;

	// Compute outcodes for start, end, and whatever point lies outside the clip rectangle.
	OutCode outcode0 = ComputeOutCode(start, min, max);
	OutCode outcode1 = ComputeOutCode(end, min, max);
	bool accept = false;

	while (true) {
		if (!(outcode0 | outcode1)) {
			// Bitwise OR is 0: both points inside window, trivially accept and exit loop.
			accept = true;
			break;
		}
		else if (outcode0 & outcode1) {
			// Bitwise AND is not 0: both points share an outside zone (LEFT, RIGHT, TOP,
			// or BOTTOM), so both must be outside window, exit loop (accept is false).
			break;
		}
		else {
			// Failed both tests, so calculate the line segment to clip
			// from an outside point to an intersection with clip edge.
			double x, y;

			// At least one endpoint is outside the clip rectangle, pick it.
			OutCode outcodeOut = outcode1 > outcode0 ? outcode1 : outcode0;

			// Now find the intersection point.
			// Use formulas:
			//   slope = (y1 - y0) / (x1 - x0)
			//   x = x0 + (1 / slope) * (ym - y0), where ym is ymin or ymax
			//   y = y0 + slope * (xm - x0), where xm is xmin or xmax
			// No need to worry about divide-by-zero because, in each case, the
			// outcode bit being tested guarantees the denominator is non-zero.
			if (outcodeOut & TOP)
			{
				x = start.x + (end.x - start.x) * (max.y - start.y) / (end.y - start.y);
				y = max.y;
			}
			else if (outcodeOut & BOTTOM)
			{
				x = start.x + (end.x - start.x) * (min.y - start.y) / (end.y - start.y);
				y = min.y;
			}
			else if (outcodeOut & RIGHT)
			{
				y = start.y + (end.y - start.y) * (max.x - start.x) / (end.x - start.x);
				x = max.x;
			}
			else if (outcodeOut & LEFT)
			{
				y = start.y + (end.y - start.y) * (min.x - start.x) / (end.x - start.x);
				x = min.x;
			}

			// Now we move outside point to intersection point to clip and get ready for next pass.
			if (outcodeOut == outcode0) {
				start.x = x;
				start.y = y;
				outcode0 = ComputeOutCode(start, min, max);
			}
			else {
				end.x = x;
				end.y = y;
				outcode1 = ComputeOutCode(end, min, max);
			}
		}
	}
	return accept;
}
