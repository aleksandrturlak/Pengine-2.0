#pragma once

#include "ComponentSystems/ComponentSystem.h"
#include "InventoryComponent.h"
#include <map>
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Pengine { class Entity; class Scene; }

class LootSystem : public Pengine::ComponentSystem
{
public:
	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return {}; }

	// Spawn a physical loot crate at world position with randomized items.
	// isSmall=true uses crate-small (player/enemy drops); false uses crate-medium (level spawns).
	static std::shared_ptr<Pengine::Entity> SpawnLootContainer(
		std::shared_ptr<Pengine::Scene> scene,
		glm::vec3 position,
		int raidDepth,
		int minItems = 1,
		int maxItems = 3,
		bool isSmall = false);

	// Generate a random loot table based on raid depth
	static std::vector<InventorySlot> GenerateLoot(int raidDepth, int count);

	// Generate one random item
	static InventorySlot GenerateRandomItem(int raidDepth);

	// Drop an inventory grid item to the world
	static void DropItemToWorld(
		InventoryComponent& inv, int gridRow, int gridCol,
		std::shared_ptr<Pengine::Entity> playerEntity,
		std::shared_ptr<Pengine::Scene> scene);

	// Drop an arbitrary slot (not from grid) to the world
	static void DropSlotToWorld(
		const InventorySlot& slot,
		std::shared_ptr<Pengine::Entity> playerEntity,
		std::shared_ptr<Pengine::Scene> scene);
};
