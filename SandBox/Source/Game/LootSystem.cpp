#include "LootSystem.h"

#include "LootContainerComponent.h"
#include "InteractableComponent.h"
#include "ItemComponent.h"
#include "InventoryComponent.h"
#include "GameStateComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/Serializer.h"

#include "Components/Transform.h"
#include <cstdlib>
#include <algorithm>

void LootSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	// Spin non-looted containers; delete empty ones.
	auto view = scene->GetRegistry().view<LootContainerComponent>();
	std::vector<std::shared_ptr<Pengine::Entity>> toDelete;
	for (auto handle : view)
	{
		auto& lc = view.get<LootContainerComponent>(handle);
		auto entity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
		if (!entity) continue;

		if (lc.items.empty())
		{
			toDelete.push_back(entity);
			continue;
		}
	}
	for (auto& e : toDelete)
		scene->DeleteEntity(e);
}

std::shared_ptr<Pengine::Entity> LootSystem::SpawnLootContainer(
	std::shared_ptr<Pengine::Scene> scene,
	glm::vec3 position,
	int raidDepth,
	int minItems,
	int maxItems,
	bool isSmall)
{
	const char* prefabPath = isSmall
		? "Game/Assets/crate-small/crate-small.prefab"
		: "Game/Assets/crate-medium/crate-medium.prefab";

	auto entity = Pengine::Serializer::DeserializePrefab(prefabPath, scene);
	if (!entity) return nullptr;

	auto& t = entity->GetComponent<Pengine::Transform>();
	t.Translate({ position.x, 0.0f, position.z });

	auto& lc = entity->AddComponent<LootContainerComponent>();
	int count = minItems + std::rand() % std::max(1, maxItems - minItems + 1);
	if (count > 0)
		lc.items = GenerateLoot(raidDepth, count);
	lc.label = "Loot";

	auto& ic = entity->AddComponent<InteractableComponent>();
	ic.interactTypeInt = static_cast<int>(InteractableComponent::InteractType::LootContainer);
	ic.radius          = 2.2f;
	ic.label           = "[E] Loot";

	return entity;
}

std::vector<InventorySlot> LootSystem::GenerateLoot(int raidDepth, int count)
{
	std::vector<InventorySlot> result;
	result.reserve(count);
	for (int i = 0; i < count; ++i)
		result.push_back(GenerateRandomItem(raidDepth));
	return result;
}

InventorySlot LootSystem::GenerateRandomItem(int raidDepth)
{
	// Item type probabilities: Heal 30%, Ammo 30%, Armor 15%, Weapon 10%, Backpack 5%, Credits 10%
	int roll = std::rand() % 100;
	InventorySlot slot;
	slot.occupied = true;

	auto setRarity = [&](InventorySlot& s)
	{
		int depth = std::min(raidDepth, 5);
		int r2 = std::rand() % 100;
		// Higher depth = better rarity chance
		int epicThresh    = depth * 4;
		int rareThresh    = epicThresh + depth * 8;
		int uncommonThresh = rareThresh + 25;
		if (r2 < epicThresh)
			s.rarityInt = static_cast<int>(ItemComponent::Rarity::Epic);
		else if (r2 < rareThresh)
			s.rarityInt = static_cast<int>(ItemComponent::Rarity::Rare);
		else if (r2 < uncommonThresh)
			s.rarityInt = static_cast<int>(ItemComponent::Rarity::Uncommon);
		else
			s.rarityInt = static_cast<int>(ItemComponent::Rarity::Common);
	};

	float depthMult = 1.0f + raidDepth * 0.3f;

	if (roll < 30) // Heal
	{
		slot.itemTypeInt = static_cast<int>(ItemComponent::Type::Heal);
		slot.itemName    = "Medkit";
		slot.value       = 30.0f * depthMult;
		slot.creditValue = 20;
		setRarity(slot);
	}
	else if (roll < 60) // Ammo
	{
		static const char* kAmmoSuffix[]  = { " P", " R", " S" };
		static const int   kAmmoStack[]   = { 15, 30, 10 };
		int ammoType = std::rand() % 3;
		slot.itemTypeInt = static_cast<int>(ItemComponent::Type::Ammo);
		slot.ammoTypeInt = ammoType;
		slot.ammoCount   = kAmmoStack[ammoType];
		slot.itemName    = std::string("Ammo Pack") + kAmmoSuffix[ammoType];
		slot.creditValue = 10;
		slot.rarityInt   = static_cast<int>(ItemComponent::Rarity::Common);
	}
	else if (roll < 75) // Armor
	{
		slot.itemTypeInt = static_cast<int>(ItemComponent::Type::Armor);
		setRarity(slot);
		int rarity = slot.rarityInt;
		float qual = 1.0f + rarity * 0.5f + raidDepth * 0.1f;
		slot.armorDamageReduction = std::min(0.1f + rarity * 0.08f, 0.6f);
		slot.armorMaxHealthBonus  = 20.0f * qual;
		slot.armorMoveSpeedBonus  = (rarity >= 2) ? (rarity - 1) * 0.3f : 0.0f;
		slot.creditValue          = 50 * (rarity + 1);
		const char* names[] = { "Light Vest", "Combat Armor", "Heavy Plate", "Prototype Suit" };
		slot.itemName = names[std::min(rarity, 3)];
	}
	else if (roll < 85) // Weapon
	{
		// Pick a random weapon prefab
		static const char* weaponPrefabs[] = {
			"Game/Assets/blaster-a/blaster-a.prefab",
			"Game/Assets/blaster-d/blaster-d.prefab",
			"Game/Assets/blaster-l/blaster-l.prefab",
		};
		int idx = std::rand() % 3;
		slot.itemTypeInt      = static_cast<int>(ItemComponent::Type::Weapon);
		slot.weaponPrefabName = weaponPrefabs[idx];
		slot.itemName         = (idx == 0) ? "Blaster A" : (idx == 1) ? "Blaster D" : "Blaster L";
		slot.creditValue      = 150 + raidDepth * 20;
		setRarity(slot);
	}
	else if (roll < 90) // Backpack
	{
		slot.itemTypeInt = static_cast<int>(ItemComponent::Type::Backpack);
		setRarity(slot);
		int rarity = slot.rarityInt;
		slot.backpackRows = 3 + rarity;
		slot.backpackCols = 3 + rarity;
		slot.creditValue  = 80 * (rarity + 1);
		const char* names[] = { "Small Pack", "Field Pack", "Tactical Pack", "Military Rig" };
		slot.itemName = names[std::min(rarity, 3)];
	}
	else // Credits
	{
		slot.itemTypeInt = static_cast<int>(ItemComponent::Type::Credits);
		slot.itemName    = "Credits";
		slot.value       = static_cast<float>(20 + std::rand() % (30 + raidDepth * 10));
		slot.creditValue = static_cast<int>(slot.value);
	}

	return slot;
}

void LootSystem::DropItemToWorld(
	InventoryComponent& inv, int gridRow, int gridCol,
	std::shared_ptr<Pengine::Entity> playerEntity,
	std::shared_ptr<Pengine::Scene> scene)
{
	int flatIdx = gridRow * InventoryComponent::kMaxGridCols + gridCol;
	if (flatIdx < 0 || flatIdx >= InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols)
		return;
	auto& slot = inv.grid[flatIdx];
	if (!slot.occupied) return;

	InventorySlot copy = slot;
	inv.grid[flatIdx] = {};  // zero directly — ClearGridSlot rejects out-of-bounds rows/cols
	DropSlotToWorld(copy, playerEntity, scene);
}

void LootSystem::DropSlotToWorld(
	const InventorySlot& slot,
	std::shared_ptr<Pengine::Entity> playerEntity,
	std::shared_ptr<Pengine::Scene> scene)
{
	glm::vec3 playerPos(0.0f);
	if (playerEntity && playerEntity->HasComponent<Pengine::Transform>())
		playerPos = playerEntity->GetComponent<Pengine::Transform>().GetPosition();

	// Find the nearest non-open loot container within 3 units
	constexpr float kSearchRadius = 3.0f;
	std::shared_ptr<Pengine::Entity> target;
	float bestDist = kSearchRadius + 1.0f;

	auto view = scene->GetRegistry().view<LootContainerComponent>();
	for (auto handle : view)
	{
		auto& lc = view.get<LootContainerComponent>(handle);
		if (lc.isOpen) continue;
		auto ent = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
		if (!ent) continue;
		float dist = glm::length(ent->GetComponent<Pengine::Transform>().GetPosition() - playerPos);
		if (dist < bestDist) { bestDist = dist; target = ent; }
	}

	if (!target)
	{
		glm::vec3 spawnPos = playerPos + glm::vec3(0.0f, 0.0f, -1.5f);
		target = SpawnLootContainer(scene, spawnPos, 1, 0, 0, true);
		if (target && target->HasComponent<LootContainerComponent>())
			target->GetComponent<LootContainerComponent>().items.clear();
	}

	if (target && target->HasComponent<LootContainerComponent>())
		target->GetComponent<LootContainerComponent>().items.push_back(slot);
}
