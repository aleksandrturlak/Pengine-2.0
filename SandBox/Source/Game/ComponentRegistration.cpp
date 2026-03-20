// Component Registration.

#include "WaveComponent.h"
#include "ProjectileComponent.h"
#include "EnemyComponent.h"
#include "PickupComponent.h"
#include "PlayerComponent.h"
#include "WeaponComponent.h"
#include "ItemComponent.h"
#include "InteractableComponent.h"
#include "GameStateComponent.h"
#include "PatrolComponent.h"
#include "ShopComponent.h"
#include "LootContainerComponent.h"

REGISTER_CLASS(WaveComponent)
REGISTER_CLASS(ProjectileComponent)
REGISTER_CLASS(EnemyComponent)
REGISTER_CLASS(PickupComponent)
REGISTER_CLASS(PlayerComponent)
REGISTER_CLASS(WeaponComponent)
REGISTER_CLASS(ItemComponent)
REGISTER_CLASS(InteractableComponent)
REGISTER_CLASS(GameStateComponent)
REGISTER_CLASS(PatrolComponent)
REGISTER_CLASS(ShopComponent)
REGISTER_CLASS(LootContainerComponent)

// InventoryComponent contains std::array with no PROPERTY macros — managed in code only.
