#include "PickupSystem.h"

#include "PickupComponent.h"
#include "PlayerComponent.h"
#include "InventoryComponent.h"
#include "ItemComponent.h"
#include "InventorySystem.h"

#include "Core/Scene.h"
#include "Core/Entity.h"

#include "Components/Transform.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

#include <algorithm>
#include <vector>

void PickupSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto playerEntity = scene->FindEntityByName("Player");
	if (!playerEntity || !playerEntity->HasComponent<PlayerComponent>())
		return;

	PlayerComponent& pc = playerEntity->GetComponent<PlayerComponent>();
	if (!pc.isAlive)
		return;

	glm::vec3 playerPos = playerEntity->GetComponent<Pengine::Transform>().GetPosition();

	std::vector<std::shared_ptr<Pengine::Entity>> toDestroy;

	auto view = scene->GetRegistry().view<PickupComponent>();
	for (auto handle : view)
	{
		PickupComponent& pickup = view.get<PickupComponent>(handle);

		auto entity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
		if (!entity)
			continue;

		auto& transform = entity->GetComponent<Pengine::Transform>();

		// Capture spawn height on first frame
		if (pickup.baseY == 0.0f)
			pickup.baseY = transform.GetPosition().y;

		// Spin around Y axis
		pickup.spinAngle += pickup.spinSpeed * dt;
		if (pickup.spinAngle > glm::two_pi<float>())
			pickup.spinAngle -= glm::two_pi<float>();
		transform.Rotate(glm::vec3(0.0f, pickup.spinAngle, 0.0f));

		// Bob up and down
		pickup.bobTime += pickup.bobSpeed * dt;
		float newY = pickup.baseY + std::sin(pickup.bobTime) * pickup.bobAmplitude;
		glm::vec3 pos = transform.GetPosition();
		transform.Translate(glm::vec3(pos.x, newY, pos.z));

		glm::vec3 pickupPos = transform.GetPosition();
		float dist = glm::length(playerPos - pickupPos);
		if (dist > pickup.pickupRadius)
			continue;

		if (pickup.type == PickupComponent::Type::Health)
		{
			pc.health = std::min(pc.health + pickup.value, pc.maxHealth);
		}
		else // Ammo — add as inventory grid item
		{
			if (playerEntity->HasComponent<InventoryComponent>())
			{
				auto& inv = playerEntity->GetComponent<InventoryComponent>();
				InventorySlot ammoSlot;
				ammoSlot.occupied    = true;
				static const char* kSuffix[] = { " P", " R", " S" };
				ammoSlot.itemName = std::string("Ammo Pack") +
					((pickup.ammoTypeInt >= 0 && pickup.ammoTypeInt < 3) ? kSuffix[pickup.ammoTypeInt] : "");
				ammoSlot.itemTypeInt = static_cast<int>(ItemComponent::Type::Ammo);
				ammoSlot.ammoTypeInt = pickup.ammoTypeInt;
				ammoSlot.ammoCount   = pickup.ammoCount;
				if (!InventorySystem::GiveItem(inv, ammoSlot, nullptr, nullptr))
					continue; // inventory full — leave pickup in the world
			}
		}

		toDestroy.push_back(entity);
	}

	for (auto& e : toDestroy)
		scene->DeleteEntity(e);
}
