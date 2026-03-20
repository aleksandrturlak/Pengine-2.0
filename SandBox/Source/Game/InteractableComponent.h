#pragma once

#include <string>
#include "Core/ReflectionSystem.h"

// Marks an entity as interactable (press E when nearby).
// InteractableSystem detects proximity and sets the player's nearInteractable.
struct InteractableComponent
{
	enum class InteractType
	{
		Shop,           // 0
		Stash,          // 1
		Extraction,     // 2 — in-raid extraction portal
		LootContainer,  // 3
		RaidPortal,     // 4 — HomeBase portal to start a raid
	};

	PROPERTY(int,         interactTypeInt, 0)     // cast to InteractType
	PROPERTY(float,       radius,          2.5f)  // interaction distance
	PROPERTY(std::string, label,           "Interact") // shown in HUD prompt
	PROPERTY(bool,        isOpen,          false) // used by loot containers
	PROPERTY(bool,        isLooted,        false)

	InteractType GetInteractType() const { return static_cast<InteractType>(interactTypeInt); }
};
