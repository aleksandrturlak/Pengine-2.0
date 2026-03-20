#include "ShopSystem.h"

#include "ShopComponent.h"
#include "InventoryComponent.h"
#include "InventorySystem.h"
#include "ItemComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"

#include <cstdlib>

void ShopSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	// Populate catalogs for any trader entities that haven't been initialized yet
	auto view = scene->GetRegistry().view<ShopComponent>();
	for (auto handle : view)
	{
		auto& shop = view.get<ShopComponent>(handle);
		if (shop.catalog.empty())
			PopulateShop(shop);
	}
}

void ShopSystem::PopulateShop(ShopComponent& shop)
{
	shop.catalog.clear();

	// Fixed shop inventory for HomeBase
	auto add = [&](InventorySlot s) { shop.catalog.push_back(s); };

	// Heals
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Medkit";       s.itemTypeInt = static_cast<int>(ItemComponent::Type::Heal);
		s.value = 30.0f;             s.creditValue = 40;
		add(s);
	}
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Large Medkit"; s.itemTypeInt = static_cast<int>(ItemComponent::Type::Heal);
		s.value = 60.0f;             s.creditValue = 75;
		add(s);
	}

	// Ammo — Pistol (P), Rifle (R), Shotgun (S)
	{
		InventorySlot s; s.occupied = true;
		s.itemTypeInt = static_cast<int>(ItemComponent::Type::Ammo);
		s.itemName = "Ammo Pack P"; s.ammoTypeInt = 0; s.ammoCount = 15; s.creditValue = 20; add(s);
		s.itemName = "Ammo Pack R"; s.ammoTypeInt = 1; s.ammoCount = 30; s.creditValue = 25; add(s);
		s.itemName = "Ammo Pack S"; s.ammoTypeInt = 2; s.ammoCount = 10; s.creditValue = 30; add(s);
	}

	// Armor
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Light Vest";
		s.itemTypeInt = static_cast<int>(ItemComponent::Type::Armor);
		s.armorDamageReduction = 0.10f; // -10% damage
		s.armorMoveSpeedBonus  = 0.0f;
		s.creditValue = 100;
		add(s);
	}
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Combat Armor";
		s.itemTypeInt = static_cast<int>(ItemComponent::Type::Armor);
		s.armorDamageReduction = 0.20f; // -20% damage
		s.armorMoveSpeedBonus  = 0.0f;
		s.creditValue = 220;
		add(s);
	}

	// Backpacks
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Small Pack";
		s.itemTypeInt = static_cast<int>(ItemComponent::Type::Backpack);
		s.backpackRows = 3; s.backpackCols = 4;
		s.creditValue = 80;
		add(s);
	}
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Field Pack";
		s.itemTypeInt = static_cast<int>(ItemComponent::Type::Backpack);
		s.backpackRows = 4; s.backpackCols = 4;
		s.creditValue = 180;
		add(s);
	}

	// Weapons
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Blaster A";
		s.itemTypeInt = static_cast<int>(ItemComponent::Type::Weapon);
		s.weaponPrefabName = "Game/Assets/blaster-a/blaster-a.prefab";
		s.creditValue = 200;
		add(s);
	}
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Blaster D";
		s.itemTypeInt = static_cast<int>(ItemComponent::Type::Weapon);
		s.weaponPrefabName = "Game/Assets/blaster-d/blaster-d.prefab";
		s.creditValue = 120;
		add(s);
	}
	{
		InventorySlot s; s.occupied = true;
		s.itemName = "Blaster L";
		s.itemTypeInt = static_cast<int>(ItemComponent::Type::Weapon);
		s.weaponPrefabName = "Game/Assets/blaster-l/blaster-l.prefab";
		s.creditValue = 180;
		add(s);
	}
}

bool ShopSystem::BuyItem(ShopComponent& shop, int catalogIndex, InventoryComponent& inv,
                         std::shared_ptr<Pengine::Entity> playerEntity,
                         std::shared_ptr<Pengine::Scene> scene)
{
	if (catalogIndex < 0 || catalogIndex >= static_cast<int>(shop.catalog.size()))
		return false;

	const InventorySlot& item = shop.catalog[catalogIndex];
	if (inv.credits < item.creditValue)
		return false;

	if (!InventorySystem::GiveItem(inv, item, playerEntity, scene))
		return false;

	inv.credits -= item.creditValue;
	return true;
}

bool ShopSystem::SellItem(InventoryComponent& inv, int gridRow, int gridCol)
{
	if (gridRow < 0 || gridRow >= inv.currentRows || gridCol < 0 || gridCol >= inv.currentCols)
		return false;

	auto& slot = inv.grid[gridRow * InventoryComponent::kMaxGridCols + gridCol];
	if (!slot.occupied) return false;

	// Sell for 50% of credit value
	inv.credits += std::max(1, slot.creditValue / 2);
	inv.ClearGridSlot(gridRow, gridCol);
	return true;
}
