#pragma once

#include <string>
#include <vector>
#include <array>
#include "Core/ReflectionSystem.h"

// Represents an item slot entry stored inside the inventory.
struct InventorySlot
{
	bool        occupied    = false;
	std::string itemName;         // display name
	int         itemTypeInt  = 0; // ItemComponent::Type
	int         rarityInt    = 0; // ItemComponent::Rarity
	int         creditValue  = 0;
	float       value        = 0.0f;

	// Armor/backpack specific
	float armorDamageReduction = 0.0f;
	float armorMaxHealthBonus  = 0.0f;
	float armorMoveSpeedBonus  = 0.0f;
	int   armorSlotType        = 0;
	int   backpackRows         = 3;
	int   backpackCols         = 3;

	// Weapon specific
	std::string weaponPrefabName;

	// Ammo specific (itemTypeInt == Ammo)
	int ammoTypeInt = 0; // ItemComponent::AmmoType
	int ammoCount   = 0; // rounds in this stack
};

// Attached to the Player entity.
// Owns all item data for the player's inventory, equipment, and weapon slots.
struct InventoryComponent
{
	static constexpr int kMaxWeaponSlots  = 2;
	static constexpr int kMaxGridRows     = 6;
	static constexpr int kMaxGridCols     = 6;

	static constexpr int kAmmoTypes = 3; // Pistol, Rifle, Shotgun

	// Weapon slots (indices 0–1) — stores weapon prefab names and display names
	std::array<std::string, kMaxWeaponSlots> weaponSlots = {};
	std::array<std::string, kMaxWeaponSlots> weaponSlotNames = {};
	std::array<bool, kMaxWeaponSlots>        weaponSlotOccupied = {};

	// Reserve ammo per type (drawn on reload)
	std::array<int, kAmmoTypes> ammo = {};

	// Equipment slots
	InventorySlot armorSlot;
	InventorySlot backpackSlot;

	// Grid inventory — dimensions defined by equipped backpack
	// Flat array [row * cols + col]
	int currentRows = 2;
	int currentCols = 2;
	std::array<InventorySlot, kMaxGridRows * kMaxGridCols> grid = {};

	// Credits
	int credits = 500;

	// UI state
	bool inventoryOpen = false;

	// Pending interaction (entity name of interactable the player is near)
	std::string nearInteractable;

	// Helper: returns true if a grid slot is valid and unoccupied
	bool IsGridSlotFree(int row, int col) const
	{
		if (row < 0 || row >= currentRows || col < 0 || col >= currentCols) return false;
		return !grid[row * kMaxGridCols + col].occupied;
	}

	// Helper: returns true if at least one grid slot is free
	bool HasFreeGridSlot() const
	{
		for (int r = 0; r < currentRows; ++r)
			for (int c = 0; c < currentCols; ++c)
				if (!grid[r * kMaxGridCols + c].occupied) return true;
		return false;
	}

	// Helper: add item to first free grid slot. Returns true on success.
	bool AddToGrid(const InventorySlot& slot)
	{
		for (int r = 0; r < currentRows; ++r)
			for (int c = 0; c < currentCols; ++c)
				if (!grid[r * kMaxGridCols + c].occupied)
				{
					grid[r * kMaxGridCols + c] = slot;
					grid[r * kMaxGridCols + c].occupied = true;
					return true;
				}
		return false;
	}

	// Zero all cells that are outside the current active grid dimensions.
	// Must be called before saving the inventory to persistent state.
	void SanitizeGrid()
	{
		for (int r = 0; r < kMaxGridRows; ++r)
			for (int c = 0; c < kMaxGridCols; ++c)
				if (r >= currentRows || c >= currentCols)
					grid[r * kMaxGridCols + c] = {};
	}

	// Helper: remove item at grid slot
	void ClearGridSlot(int row, int col)
	{
		if (row < 0 || row >= currentRows || col < 0 || col >= currentCols) return;
		grid[row * kMaxGridCols + col] = {};
	}
};
