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
			void* sender = nullptr)
			: Event(type, sender)
			, m_EntityA(std::move(entityA))
			, m_EntityB(std::move(entityB))
		{
		}

		[[nodiscard]] std::shared_ptr<class Entity> GetEntityA() const { return m_EntityA; }
		[[nodiscard]] std::shared_ptr<class Entity> GetEntityB() const { return m_EntityB; }

	private:
		std::shared_ptr<class Entity> m_EntityA;
		std::shared_ptr<class Entity> m_EntityB;
	};

}
