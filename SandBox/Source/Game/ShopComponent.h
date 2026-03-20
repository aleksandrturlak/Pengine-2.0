#pragma once

#include <vector>
#include "Core/ReflectionSystem.h"
#include "InventoryComponent.h"

// Attached to the trader NPC entity.
// Holds a catalog of items for sale and accepts player sell-backs.
struct ShopComponent
{
	std::vector<InventorySlot> catalog;  // items available to buy

	PROPERTY(bool, isOpen, false)
	PROPERTY(std::string, shopName, "Trader")
};
