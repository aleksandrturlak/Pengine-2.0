#include "InventorySystem.h"

#include "InventoryComponent.h"
#include "ItemComponent.h"
#include "PlayerComponent.h"
#include "WeaponComponent.h"
#include "RoguelikeState.h"
#include "LootSystem.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/Serializer.h"
#include "Core/Logger.h"
#include "Core/WindowManager.h"
#include "Core/Window.h"
#include "Core/Input.h"
#include "Core/KeyCode.h"

#include "Components/Transform.h"

#include <algorithm>

void InventorySystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto playerEntity = scene->FindEntityByName("Player");
	if (!playerEntity || !playerEntity->HasComponent<InventoryComponent>()) return;

	auto& inv = playerEntity->GetComponent<InventoryComponent>();

	auto window = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
	if (!window) return;
	auto& input = Pengine::Input::GetInstance(window.get());

	// Toggle inventory on Tab
	if (input.IsKeyPressed(Pengine::KeyCode::KEY_TAB))
	{
		inv.inventoryOpen = !inv.inventoryOpen;
		if (inv.inventoryOpen)
			window->ShowCursor();
		else
		{
			// Only re-capture cursor if player is alive and not in any other UI
			if (playerEntity->HasComponent<PlayerComponent>()
				&& playerEntity->GetComponent<PlayerComponent>().isAlive)
				window->DisableCursor();
		}
	}

	// Keep equipment stats applied each frame (armor/backpack might change)
	ApplyEquipmentStats(inv, playerEntity);
}

bool InventorySystem::GiveItem(InventoryComponent& inv, const InventorySlot& item,
                               std::shared_ptr<Pengine::Entity> playerEntity,
                               std::shared_ptr<Pengine::Scene> scene)
{
	using Type = ItemComponent::Type;
	Type t = static_cast<Type>(item.itemTypeInt);

	// Instant-consume items
	if (t == Type::Credits)
	{
		inv.credits += static_cast<int>(item.value);
		return true;
	}
	if (t == Type::Ammo)
	{
		// Stack limits per ammo type: Pistol=15, Rifle=30, Shotgun=10
		static constexpr int kStackLimit[] = { 15, 30, 10 };
		int stackLimit = (item.ammoTypeInt >= 0 && item.ammoTypeInt < 3)
			? kStackLimit[item.ammoTypeInt] : 15;

		// Try to stack onto an existing partial ammo slot of the same type
		for (int i = 0; i < InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols; ++i)
		{
			auto& s = inv.grid[i];
			if (!s.occupied) continue;
			if (static_cast<ItemComponent::Type>(s.itemTypeInt) != Type::Ammo) continue;
			if (s.ammoTypeInt != item.ammoTypeInt) continue;
			if (s.ammoCount < stackLimit)
			{
				s.ammoCount = std::min(s.ammoCount + item.ammoCount, stackLimit);
				return true;
			}
		}
		// No partial stack — need a free slot
		if (!inv.HasFreeGridSlot()) return false;
		InventorySlot newSlot   = item;
		newSlot.occupied        = true;
		newSlot.ammoCount       = std::min(item.ammoCount, stackLimit);
		return inv.AddToGrid(newSlot);
	}

	// Weapons auto-equip to first free weapon slot; fall back to grid if full
	if (t == Type::Weapon)
	{
		// Check if there's a free weapon slot; if not, need a grid slot
		bool hasFreeWeaponSlot = false;
		for (int s = 0; s < InventoryComponent::kMaxWeaponSlots; ++s)
			if (!inv.weaponSlotOccupied[s]) { hasFreeWeaponSlot = true; break; }
		if (!hasFreeWeaponSlot && !inv.HasFreeGridSlot()) return false;

		for (int s = 0; s < InventoryComponent::kMaxWeaponSlots; ++s)
		{
			if (!inv.weaponSlotOccupied[s])
			{
				inv.weaponSlots[s]        = item.weaponPrefabName;
				inv.weaponSlotNames[s]    = item.itemName;
				inv.weaponSlotOccupied[s] = true;
				// Spawn the weapon entity if we have a scene and player camera
				if (scene && playerEntity)
				{
					auto camera = playerEntity->FindEntityInHierarchy("PlayerCamera");
					if (camera)
					{
						auto weaponEnt = Pengine::Serializer::DeserializePrefab(item.weaponPrefabName, scene);
						if (weaponEnt)
						{
							auto& wc = weaponEnt->GetComponent<WeaponComponent>();
							wc.slot = s;
							camera->AddChild(weaponEnt, false);
							weaponEnt->GetComponent<Pengine::Transform>().Translate(wc.restPosition);
						}
					}
				}
				return true;
			}
		}
		// All slots full — put in grid
		return inv.AddToGrid(item);
	}

	// Everything else goes to the grid
	return inv.AddToGrid(item);
}

bool InventorySystem::EquipWeapon(InventoryComponent& inv, int gridRow, int gridCol, int weaponSlot,
                                  std::shared_ptr<Pengine::Entity> playerEntity,
                                  std::shared_ptr<Pengine::Scene> scene)
{
	if (weaponSlot < 0 || weaponSlot >= InventoryComponent::kMaxWeaponSlots) return false;
	auto& gridSlot = inv.grid[gridRow * InventoryComponent::kMaxGridCols + gridCol];
	if (!gridSlot.occupied) return false;
	if (static_cast<ItemComponent::Type>(gridSlot.itemTypeInt) != ItemComponent::Type::Weapon) return false;

	// Swap current weapon out to inventory grid if occupied
	if (inv.weaponSlotOccupied[weaponSlot])
	{
		InventorySlot old;
		old.occupied         = true;
		old.itemName         = inv.weaponSlotNames[weaponSlot];
		old.itemTypeInt      = static_cast<int>(ItemComponent::Type::Weapon);
		old.weaponPrefabName = inv.weaponSlots[weaponSlot];
		old.creditValue      = 100;
		inv.AddToGrid(old);
	}

	inv.weaponSlots[weaponSlot]        = gridSlot.weaponPrefabName;
	inv.weaponSlotNames[weaponSlot]    = gridSlot.itemName;
	inv.weaponSlotOccupied[weaponSlot] = true;
	inv.ClearGridSlot(gridRow, gridCol);

	// Respawn weapon entity as child of PlayerCamera
	auto camera = playerEntity->FindEntityInHierarchy("PlayerCamera");
	if (camera)
	{
		// Remove old weapon in that slot
		std::vector<std::shared_ptr<Pengine::Entity>> toRemove;
		for (auto& weakChild : camera->GetChilds())
		{
			auto child = weakChild.lock();
			if (child && child->HasComponent<WeaponComponent>()
				&& child->GetComponent<WeaponComponent>().slot == weaponSlot)
				toRemove.push_back(child);
		}
		for (auto& e : toRemove)
			scene->DeleteEntity(e);

		// Spawn new weapon
		auto weaponEnt = Pengine::Serializer::DeserializePrefab(inv.weaponSlots[weaponSlot], scene);
		if (weaponEnt)
		{
			auto& wc = weaponEnt->GetComponent<WeaponComponent>();
			wc.slot = weaponSlot;
			camera->AddChild(weaponEnt, false);
			weaponEnt->GetComponent<Pengine::Transform>().Translate(wc.restPosition);
		}
	}

	return true;
}

void InventorySystem::EquipArmor(InventoryComponent& inv, int gridRow, int gridCol,
                                 std::shared_ptr<Pengine::Entity> playerEntity)
{
	auto& gridSlot = inv.grid[gridRow * InventoryComponent::kMaxGridCols + gridCol];
	if (!gridSlot.occupied) return;
	if (static_cast<ItemComponent::Type>(gridSlot.itemTypeInt) != ItemComponent::Type::Armor) return;

	// Put old armor back if any
	if (inv.armorSlot.occupied)
		inv.AddToGrid(inv.armorSlot);

	inv.armorSlot = gridSlot;
	inv.ClearGridSlot(gridRow, gridCol);
	ApplyEquipmentStats(inv, playerEntity);
}

// Drop grid items that fall outside [newRows x newCols] to the world
static void DropOverflowItems(InventoryComponent& inv, int newRows, int newCols,
	std::shared_ptr<Pengine::Entity> playerEntity,
	std::shared_ptr<Pengine::Scene> scene)
{
	for (int r = 0; r < InventoryComponent::kMaxGridRows; ++r)
		for (int c = 0; c < InventoryComponent::kMaxGridCols; ++c)
		{
			if (r < newRows && c < newCols) continue; // inside new bounds — keep
		// outside new bounds — drop and also zero the slot even if drop fails
			int flatIdx = r * InventoryComponent::kMaxGridCols + c;
			if (!inv.grid[flatIdx].occupied) continue;
			LootSystem::DropItemToWorld(inv, r, c, playerEntity, scene);
		}
}

void InventorySystem::EquipBackpack(InventoryComponent& inv, int gridRow, int gridCol,
	                                    std::shared_ptr<Pengine::Entity> playerEntity,
	                                    std::shared_ptr<Pengine::Scene> scene)
{
	auto& gridSlot = inv.grid[gridRow * InventoryComponent::kMaxGridCols + gridCol];
	if (!gridSlot.occupied) return;
	if (static_cast<ItemComponent::Type>(gridSlot.itemTypeInt) != ItemComponent::Type::Backpack) return;

	int newRows = std::clamp(gridSlot.backpackRows, 1, InventoryComponent::kMaxGridRows);
	int newCols = std::clamp(gridSlot.backpackCols, 1, InventoryComponent::kMaxGridCols);

	InventorySlot oldBackpack = inv.backpackSlot;

	inv.backpackSlot = gridSlot;
	inv.ClearGridSlot(gridRow, gridCol);
	inv.currentRows = newRows;
	inv.currentCols = newCols;

	// Drop items outside the new grid bounds
	DropOverflowItems(inv, newRows, newCols, playerEntity, scene);

	// Put old backpack back into grid; drop if no space
	if (oldBackpack.occupied && !inv.AddToGrid(oldBackpack))
		LootSystem::DropSlotToWorld(oldBackpack, playerEntity, scene);
}

bool InventorySystem::UnequipArmor(InventoryComponent& inv)
{
	if (!inv.armorSlot.occupied) return false;
	if (!inv.AddToGrid(inv.armorSlot)) return false;
	inv.armorSlot = {};
	return true;
}

bool InventorySystem::UnequipBackpack(InventoryComponent& inv,
	                                     std::shared_ptr<Pengine::Entity> playerEntity,
	                                     std::shared_ptr<Pengine::Scene> scene)
{
	if (!inv.backpackSlot.occupied) return false;

	constexpr int kDefaultRows = 2;
	constexpr int kDefaultCols = 2;

	InventorySlot bp = inv.backpackSlot;
	inv.backpackSlot = {};
	inv.currentRows = kDefaultRows;
	inv.currentCols = kDefaultCols;

	// Drop items outside 2x2
	DropOverflowItems(inv, kDefaultRows, kDefaultCols, playerEntity, scene);

	// Try to put the backpack itself into grid; drop it if no space
	if (!inv.AddToGrid(bp))
		LootSystem::DropSlotToWorld(bp, playerEntity, scene);

	return true;
}

bool InventorySystem::UnequipWeapon(InventoryComponent& inv, int weaponSlot,
                                    std::shared_ptr<Pengine::Entity> playerEntity,
                                    std::shared_ptr<Pengine::Scene> scene)
{
	if (weaponSlot < 0 || weaponSlot >= InventoryComponent::kMaxWeaponSlots) return false;
	if (!inv.weaponSlotOccupied[weaponSlot]) return false;

	InventorySlot slot;
	slot.occupied         = true;
	slot.itemName         = inv.weaponSlotNames[weaponSlot];
	slot.itemTypeInt      = static_cast<int>(ItemComponent::Type::Weapon);
	slot.weaponPrefabName = inv.weaponSlots[weaponSlot];
	slot.creditValue      = 100;

	if (!inv.AddToGrid(slot)) return false;

	inv.weaponSlots[weaponSlot]        = {};
	inv.weaponSlotNames[weaponSlot]    = {};
	inv.weaponSlotOccupied[weaponSlot] = false;

	// Remove the weapon entity from the camera
	if (playerEntity && scene)
	{
		auto camera = playerEntity->FindEntityInHierarchy("PlayerCamera");
		if (camera)
		{
			std::vector<std::shared_ptr<Pengine::Entity>> toRemove;
			for (auto& weakChild : camera->GetChilds())
			{
				auto child = weakChild.lock();
				if (child && child->HasComponent<WeaponComponent>()
					&& child->GetComponent<WeaponComponent>().slot == weaponSlot)
					toRemove.push_back(child);
			}
			for (auto& e : toRemove)
				scene->DeleteEntity(e);
		}
	}

	return true;
}

bool InventorySystem::UseItem(InventoryComponent& inv, int gridRow, int gridCol,
                              std::shared_ptr<Pengine::Entity> playerEntity)
{
	int flatIdx = gridRow * InventoryComponent::kMaxGridCols + gridCol;
	auto& slot = inv.grid[flatIdx];
	if (!slot.occupied) return false;

	using Type = ItemComponent::Type;
	if (static_cast<Type>(slot.itemTypeInt) == Type::Heal)
	{
		if (playerEntity && playerEntity->HasComponent<PlayerComponent>())
		{
			auto& pc = playerEntity->GetComponent<PlayerComponent>();
			if (pc.health >= pc.maxHealth) return false;
			pc.health = std::min(pc.health + slot.value, pc.maxHealth);
		}
		slot = {};
		return true;
	}
	return false;
}

void InventorySystem::ApplyEquipmentStats(InventoryComponent& inv,
                                          std::shared_ptr<Pengine::Entity> playerEntity)
{
	if (!playerEntity || !playerEntity->HasComponent<PlayerComponent>()) return;
	auto& pc = playerEntity->GetComponent<PlayerComponent>();

	float speedBonus = 0.0f;

	if (inv.armorSlot.occupied)
		speedBonus += inv.armorSlot.armorMoveSpeedBonus;

	pc.maxHealth = 100.0f;
	pc.health    = std::min(pc.health, pc.maxHealth);
	pc.moveSpeed = 6.0f + speedBonus;
}
