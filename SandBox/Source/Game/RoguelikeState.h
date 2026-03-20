#pragma once

#include "InventoryComponent.h"
#include <string>

// Singleton that persists player state across scene transitions.
// On raid start: snapshot player inventory from InventoryComponent.
// On extraction: restore it. On death: discard raid gains, restore pre-raid snapshot.
class RoguelikeState
{
public:
	static RoguelikeState& GetInstance()
	{
		static RoguelikeState instance;
		return instance;
	}

	// Persistent inventory (survives scene loads)
	InventoryComponent inventory;

	// Stash — stored in HomeBase between raids
	std::vector<InventorySlot> stash;

	// Raid state
	int  raidDepth      = 0;
	bool pendingRaid    = false;  // set true when player steps through raid portal
	bool pendingExtract = false;  // set true when player extracts

	// Which scene to load next
	std::string nextScene;

	void Reset()
	{
		inventory  = InventoryComponent{};
		stash.clear();
		raidDepth      = 0;
		pendingRaid    = false;
		pendingExtract = false;
		nextScene.clear();
	}

private:
	RoguelikeState()
	{
		// Start player with a default backpack
		InventorySlot bp;
		bp.occupied    = true;
		bp.itemName    = "Basic Backpack";
		bp.itemTypeInt = 3; // Backpack — matches ItemComponent::Type enum order
		// ItemComponent::Type: Weapon=0,Armor=1,Backpack=2,Ammo=3,Heal=4,Credits=5
		bp.itemTypeInt = 2;
		bp.backpackRows = 3;
		bp.backpackCols = 3;
		bp.creditValue  = 50;
		inventory.backpackSlot = bp;
		inventory.currentRows  = 3;
		inventory.currentCols  = 3;
		inventory.credits      = 500;
	}
};
