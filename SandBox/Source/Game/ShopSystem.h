#pragma once

#include "ComponentSystems/ComponentSystem.h"
#include <map>
#include <functional>
#include <memory>

namespace Pengine { class Entity; class Scene; }

struct InventoryComponent;
struct InventorySlot;
struct ShopComponent;

class ShopSystem : public Pengine::ComponentSystem
{
public:
	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return {}; }

	// Populate the shop catalog (called when HomeBase scene loads)
	static void PopulateShop(ShopComponent& shop);

	// Buy item at catalog index — deducts credits, adds to player inventory
	static bool BuyItem(ShopComponent& shop, int catalogIndex, InventoryComponent& inv,
	                    std::shared_ptr<Pengine::Entity> playerEntity,
	                    std::shared_ptr<Pengine::Scene> scene);

	// Sell item from player inventory grid slot — adds credits
	static bool SellItem(InventoryComponent& inv, int gridRow, int gridCol);
};
