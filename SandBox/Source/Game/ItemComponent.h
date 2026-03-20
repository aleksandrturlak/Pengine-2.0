#pragma once

#include <string>
#include <glm/glm.hpp>
#include "Core/ReflectionSystem.h"

// Represents any lootable/equippable item in the world or inventory.
struct ItemComponent
{
	enum class Type
	{
		Weapon,
		Armor,
		Backpack,
		Ammo,
		Heal,
		Credits,
	};

	enum class AmmoType
	{
		Pistol,   // stacks 15
		Rifle,    // stacks 30
		Shotgun,  // stacks 10
	};

	enum class Rarity
	{
		Common,
		Uncommon,
		Rare,
		Epic,
	};

	// Identity
	PROPERTY(std::string, name,        "Unknown Item")
	PROPERTY(int,         typeInt,     0)   // cast to Type
	PROPERTY(int,         rarityInt,   0)   // cast to Rarity

	// Value
	PROPERTY(int,   creditValue,  10)   // buy/sell price
	PROPERTY(float, value,        0.0f) // heal amount, ammo count, etc.

	// Armor stats (used when typeInt == Armor)
	PROPERTY(float, armorDamageReduction, 0.0f)  // 0.0–1.0 fraction
	PROPERTY(float, armorMaxHealthBonus,  0.0f)
	PROPERTY(float, armorMoveSpeedBonus,  0.0f)
	PROPERTY(int,   armorSlotType,        0)      // 0=body

	// Backpack stats (used when typeInt == Backpack)
	PROPERTY(int, backpackRows, 3)
	PROPERTY(int, backpackCols, 3)

	// Weapon reference (used when typeInt == Weapon)
	// Name of prefab file UUID string — resolved at runtime
	PROPERTY(std::string, weaponPrefabName, "")

	// Inventory position (set by InventorySystem, -1 = not in grid)
	PROPERTY(int, inventoryRow, -1)
	PROPERTY(int, inventoryCol, -1)

	// Is this item currently equipped (armor/backpack slot)
	PROPERTY(bool, isEquipped, false)

	// Visual — entity is a 3D spinning pickup in the world;
	// when picked up the entity is deleted and item lives in inventory
	PROPERTY(float, spinAngle,     0.0f)
	PROPERTY(float, spinSpeed,     2.0f)
	PROPERTY(float, bobTime,       0.0f)
	PROPERTY(float, bobSpeed,      1.5f)
	PROPERTY(float, bobAmplitude,  0.12f)
	PROPERTY(float, baseY,         0.0f)
	PROPERTY(float, pickupRadius,  1.8f)
	PROPERTY(bool,  isWorldItem,   true)  // false when in inventory/stash

	Type   GetType()   const { return static_cast<Type>(typeInt); }
	Rarity GetRarity() const { return static_cast<Rarity>(rarityInt); }
};
