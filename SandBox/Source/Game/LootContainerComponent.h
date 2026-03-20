#pragma once

#include <vector>
#include <string>
#include "Core/ReflectionSystem.h"
#include "InventoryComponent.h"

// A loot container (chest, enemy death stash, etc.) that holds inventory slots.
// When the player opens it via InteractableSystem the loot UI shows its contents.
struct LootContainerComponent
{
	std::vector<InventorySlot> items;

	PROPERTY(bool,        isOpen,   false)
	PROPERTY(bool,        isLooted, false)
	PROPERTY(std::string, label,    "Loot")
};
