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
		inventory.currentRows  = 2;
		inventory.currentCols  = 2;
		inventory.credits      = 500;
	}
};
