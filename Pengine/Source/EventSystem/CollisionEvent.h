#pragma once

#include "../Core/Core.h"
#include "Event.h"

namespace Pengine
{

	class PENGINE_API CollisionEvent final : public Event
	{
	public:
		CollisionEvent(
			std::shared_ptr<class Entity> entityA,
			std::shared_ptr<class Entity> entityB,
			const Type type,
			const glm::vec3& contactPoint = {},
			const glm::vec3& contactNormal = {},
			void* sender = nullptr)
			: Event(type, sender)
			, m_EntityA(std::move(entityA))
			, m_EntityB(std::move(entityB))
			, m_ContactPoint(contactPoint)
			, m_ContactNormal(contactNormal)
		{
		}

		[[nodiscard]] std::shared_ptr<class Entity> GetEntityA() const { return m_EntityA; }
		[[nodiscard]] std::shared_ptr<class Entity> GetEntityB() const { return m_EntityB; }

		// World-space contact point (zero for Exit events)
		[[nodiscard]] const glm::vec3& GetContactPoint() const { return m_ContactPoint; }
		// World-space contact normal pointing from B to A (zero for Exit events)
		[[nodiscard]] const glm::vec3& GetContactNormal() const { return m_ContactNormal; }

	private:
		std::shared_ptr<class Entity> m_EntityA;
		std::shared_ptr<class Entity> m_EntityB;
		glm::vec3 m_ContactPoint{};
		glm::vec3 m_ContactNormal{};
	};

}
