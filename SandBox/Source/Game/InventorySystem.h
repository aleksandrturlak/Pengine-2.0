#pragma once

#include "ComponentSystems/ComponentSystem.h"
#include <map>
#include <functional>
#include <memory>

namespace Pengine { class Entity; class Scene; }

struct InventoryComponent;
struct InventorySlot;

class InventorySystem : public Pengine::ComponentSystem
{
public:
	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return {}; }

	// Called by InteractableSystem/LootSystem to hand an item to the player
	static bool GiveItem(InventoryComponent& inv, const InventorySlot& item,
	                     std::shared_ptr<Pengine::Entity> playerEntity,
	                     std::shared_ptr<Pengine::Scene> scene);

	// Equip weapon from inventory grid into a weapon slot
	static bool EquipWeapon(InventoryComponent& inv, int gridRow, int gridCol, int weaponSlot,
	                        std::shared_ptr<Pengine::Entity> playerEntity,
	                        std::shared_ptr<Pengine::Scene> scene);

	// Equip armor from inventory grid
	static void EquipArmor(InventoryComponent& inv, int gridRow, int gridCol,
	                       std::shared_ptr<Pengine::Entity> playerEntity);

	// Equip backpack from inventory grid
	static void EquipBackpack(InventoryComponent& inv, int gridRow, int gridCol,
	                          std::shared_ptr<Pengine::Entity> playerEntity,
	                          std::shared_ptr<Pengine::Scene> scene);

	// Unequip armor/backpack/weapon back to grid
	static bool UnequipArmor(InventoryComponent& inv);
	static bool UnequipBackpack(InventoryComponent& inv,
	                            std::shared_ptr<Pengine::Entity> playerEntity,
	                            std::shared_ptr<Pengine::Scene> scene);
	static bool UnequipWeapon(InventoryComponent& inv, int weaponSlot,
	                          std::shared_ptr<Pengine::Entity> playerEntity,
	                          std::shared_ptr<Pengine::Scene> scene);

	// Use a consumable item (Heal) from the grid — returns true if consumed
	static bool UseItem(InventoryComponent& inv, int gridRow, int gridCol,
	                    std::shared_ptr<Pengine::Entity> playerEntity);

	// Apply equipped stats to PlayerComponent
	static void ApplyEquipmentStats(InventoryComponent& inv,
	                                std::shared_ptr<Pengine::Entity> playerEntity);
};
