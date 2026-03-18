#pragma once

#include "Core.h"

namespace Pengine
{

	class PENGINE_API Raycast
	{
	public:
		struct PhysicsHit
		{
			glm::vec3 point{};
			float fraction = 1.0f;
			std::shared_ptr<class Entity> entity;
		};

		struct Hit
		{
			glm::vec3 point{};
			glm::vec3 normal{};
			glm::vec2 uv{};
			float distance = std::numeric_limits<float>::max();

			std::size_t operator()(const Hit& hit) const
			{
				return std::hash<float>{}(hit.distance);
			}

			bool operator<(const Hit& hit) const
			{
				return distance < hit.distance;
			}
		};

		static bool IntersectTriangle(
			const glm::vec3& start,
			const glm::vec3& direction,
			const glm::vec3& a,
			const glm::vec3& b,
			const glm::vec3& c,
			const glm::vec3& planeNormal,
			const float length,
			Hit& hit);

		static bool IntersectBoxOBB(
			const glm::vec3& start,
			const glm::vec3& direction,
			const glm::vec3& min,
			const glm::vec3& max,
			const glm::vec3& position,
			const glm::vec3& scale,
			const glm::mat4& rotation,
			const float length,
			Hit& hit);

		static bool IntersectBoxAABB(
			const glm::vec3& start,
			const glm::vec3& direction,
			const glm::vec3& min,
			const glm::vec3& max,
			const float length,
			Hit& hit);

		static std::map<Hit, std::shared_ptr<class Entity>> RaycastScene(
			std::shared_ptr<class Scene> scene,
			const glm::vec3& start,
			const glm::vec3& direction,
			const float length);

		static bool RaycastEntity(
			std::shared_ptr<class Entity> entity,
			const glm::vec3& start,
			const glm::vec3& direction,
			const float length,
			Hit& hit);

		static bool RaycastPhysics(
			std::shared_ptr<class Scene> scene,
			const glm::vec3& origin,
			const glm::vec3& direction,
			float length,
			PhysicsHit& hit);

		typedef int OutCode;

		/**
		 * Compute the bit code for a point (x, y) using the clip rectangle bounded diagonally by min and max.
		 */
		static OutCode ComputeOutCode(
			const glm::vec2& point,
			const glm::vec2& min,
			const glm::vec2& max);

		/**
		 * Cohen�Sutherland clipping algorithm clips a line from start to end against a rectangle with diagonal from min to max.
		 */
		static bool CohenSutherlandLineClip(
			glm::vec2 start,
			glm::vec2 end,
			const glm::vec2& min,
			const glm::vec2& max);
	};

}
