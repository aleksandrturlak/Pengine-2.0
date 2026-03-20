#include "ProjectileSystem.h"

#include "ProjectileComponent.h"
#include "PlayerComponent.h"
#include "InventoryComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"

#include "Components/Transform.h"

#include "EventSystem/EventSystem.h"
#include "EventSystem/CollisionEvent.h"

#include <algorithm>
#include <vector>

ProjectileSystem::ProjectileSystem()
{
	Pengine::EventSystem::GetInstance().RegisterClient(
		Pengine::Event::Type::OnCollisionEnter,
		{
			this,
			[this](std::shared_ptr<Pengine::Event> event)
			{
				auto* ce = static_cast<Pengine::CollisionEvent*>(event.get());
				auto entityA = ce->GetEntityA();
				auto entityB = ce->GetEntityB();

				std::shared_ptr<Pengine::Entity> projectile;
				std::shared_ptr<Pengine::Entity> other;

				if (entityA && entityA->HasComponent<ProjectileComponent>())
				{
					projectile = entityA;
					other      = entityB;
				}
				else if (entityB && entityB->HasComponent<ProjectileComponent>())
				{
					projectile = entityB;
					other      = entityA;
				}

				if (!projectile)
					return;

				// Deduplicate — same projectile might register multiple contacts per frame
				for (const auto& hit : m_PendingHits)
					if (hit.projectile == projectile)
						return;

				const bool hitPlayer = other && other->HasComponent<PlayerComponent>();
				const float damage   = projectile->GetComponent<ProjectileComponent>().damage;
				m_PendingHits.push_back({ projectile, damage, hitPlayer });
			}
		});
}

ProjectileSystem::~ProjectileSystem()
{
	Pengine::EventSystem::GetInstance().UnregisterAll(this);
}

void ProjectileSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	std::vector<std::shared_ptr<Pengine::Entity>> toDestroy;

	// Handle collision hits
	for (const auto& hit : m_PendingHits)
	{
		if (hit.hitPlayer)
		{
			auto playerEntity = scene->FindEntityByName("Player");
			if (playerEntity && playerEntity->HasComponent<PlayerComponent>())
			{
				auto& pc = playerEntity->GetComponent<PlayerComponent>();
				if (pc.isAlive)
				{
					float dmg = hit.damage;
					if (playerEntity->HasComponent<InventoryComponent>())
					{
						auto& inv = playerEntity->GetComponent<InventoryComponent>();
						if (inv.armorSlot.occupied)
							dmg *= (1.0f - inv.armorSlot.armorDamageReduction);
					}
					pc.health -= dmg;
					pc.hitFlashTimer = 0.4f;
					if (pc.health <= 0.0f)
					{
						pc.health  = 0.0f;
						pc.isAlive = false;
					}
				}
			}
		}

		toDestroy.push_back(hit.projectile);
	}
	m_PendingHits.clear();

	// Tick lifetime
	auto view = scene->GetRegistry().view<ProjectileComponent>();
	for (auto handle : view)
	{
		ProjectileComponent& proj = view.get<ProjectileComponent>(handle);
		proj.lifetime -= dt;
		if (proj.lifetime <= 0.0f)
		{
			auto entity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
			if (entity)
				toDestroy.push_back(entity);
		}
	}

	// Deduplicate (a projectile may hit and expire in the same frame) then delete
	std::sort(toDestroy.begin(), toDestroy.end());
	toDestroy.erase(std::unique(toDestroy.begin(), toDestroy.end()), toDestroy.end());
	for (auto& e : toDestroy)
		scene->DeleteEntity(e);
}
