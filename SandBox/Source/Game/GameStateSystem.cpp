#include "GameStateSystem.h"

#include "GameStateComponent.h"
#include "PlayerComponent.h"
#include "WeaponComponent.h"
#include "InventoryComponent.h"
#include "RoguelikeState.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/SceneManager.h"
#include "Core/Serializer.h"
#include "Core/Logger.h"
#include "Core/WindowManager.h"
#include "Core/Window.h"
#include "ProceduralMapSystem.h"

#include "Components/Transform.h"


void GameStateSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto gsEntity = scene->FindEntityByName("GameState");
	if (!gsEntity || !gsEntity->HasComponent<GameStateComponent>())
		return;

	GameStateComponent& gs = gsEntity->GetComponent<GameStateComponent>();
	auto& rogueState = RoguelikeState::GetInstance();

	// ── On first frame: inject persistent inventory into Player ────────────
	auto playerEntity = scene->FindEntityByName("Player");
	if (!gs.inventoryInjected && playerEntity)
	{
		auto& inv = playerEntity->HasComponent<InventoryComponent>()
			? playerEntity->GetComponent<InventoryComponent>()
			: playerEntity->AddComponent<InventoryComponent>();
		inv = rogueState.inventory;
		gs.inventoryInjected = true;

		// Sync weapon entities to match weaponSlots (removes baked-in prefab weapons if slots are empty)
		ApplyWeaponLoadout(playerEntity, scene, inv);

		// In HomeBase: capture cursor for mouse-look (freed by InteractableSystem when UI opens)
		if (gs.GetPhase() == GameStateComponent::Phase::HomeBase)
		{
			auto window = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
			if (window) window->DisableCursor();
			if (playerEntity->HasComponent<PlayerComponent>())
				playerEntity->GetComponent<PlayerComponent>().cursorDisabled = true;
		}
	}

	// ── Detect player death in raid ─────────────────────────────────────────
	if (gs.GetPhase() == GameStateComponent::Phase::InRaid && playerEntity
		&& playerEntity->HasComponent<PlayerComponent>())
	{
		auto& pc = playerEntity->GetComponent<PlayerComponent>();
		if (!pc.isAlive && !gs.playerDied)
		{
			gs.playerDied = true;
			// Wipe all gear on death, but keep credits
			int savedCredits = rogueState.inventory.credits;
			rogueState.inventory = InventoryComponent{};
			rogueState.inventory.credits = savedCredits;
			gs.SetPhase(GameStateComponent::Phase::LoadingHome);
			gs.transitionTimer = 2.5f;
			Pengine::Logger::Log("Player died — inventory wiped");
		}
	}

	// ── Extraction triggered ────────────────────────────────────────────────
	if (rogueState.pendingExtract && gs.GetPhase() == GameStateComponent::Phase::InRaid)
	{
		rogueState.pendingExtract = false;
		// Commit current inventory to persistent state
		if (playerEntity && playerEntity->HasComponent<InventoryComponent>())
		{
			auto& inv = playerEntity->GetComponent<InventoryComponent>();
			inv.SanitizeGrid();
			rogueState.inventory = inv;
		}
		gs.SetPhase(GameStateComponent::Phase::LoadingHome);
		gs.transitionTimer = 0.8f;
		Pengine::Logger::Log("Extraction! Returning to HomeBase");
	}

	// ── Raid portal triggered ───────────────────────────────────────────────
	if (rogueState.pendingRaid && gs.GetPhase() == GameStateComponent::Phase::HomeBase)
	{
		rogueState.pendingRaid = false;
		// Snapshot inventory before raid (for potential rollback)
		if (playerEntity && playerEntity->HasComponent<InventoryComponent>())
		{
			auto& inv = playerEntity->GetComponent<InventoryComponent>();
			inv.SanitizeGrid();
			rogueState.inventory = inv;
		}
		gs.SetPhase(GameStateComponent::Phase::LoadingRaid);
		gs.transitionTimer = 0.5f;
	}

	// ── Transition timers ───────────────────────────────────────────────────
	if (gs.GetPhase() == GameStateComponent::Phase::LoadingHome)
	{
		gs.transitionTimer -= dt;
		if (gs.transitionTimer <= 0.0f)
			LoadHomeBase(scene);
	}
	else if (gs.GetPhase() == GameStateComponent::Phase::LoadingRaid)
	{
		gs.transitionTimer -= dt;
		if (gs.transitionTimer <= 0.0f)
		{
			rogueState.raidDepth++;
			LoadRaid(scene, gs);
		}
	}
}

void GameStateSystem::LoadHomeBase(std::shared_ptr<Pengine::Scene> currentScene)
{
	Pengine::SceneManager::GetInstance().Delete(currentScene);
	Pengine::Serializer::DeserializeScene("Game/HomeBase.scene");
}

void GameStateSystem::LoadRaid(std::shared_ptr<Pengine::Scene> currentScene, GameStateComponent& gs)
{
	Pengine::SceneManager::GetInstance().Delete(currentScene);
	auto raidScene = Pengine::Serializer::DeserializeScene("Game/Raid.scene");
	if (!raidScene) return;

	// Add GameState to the raid scene
	auto gsEnt = raidScene->CreateEntity("GameState");
	auto& newGs = gsEnt->AddComponent<GameStateComponent>();
	newGs.SetPhase(GameStateComponent::Phase::InRaid);
	newGs.raidDepth = RoguelikeState::GetInstance().raidDepth;

	// Restore player inventory
	auto playerEnt = raidScene->FindEntityByName("Player");
	if (playerEnt)
	{
		auto& inv = playerEnt->HasComponent<InventoryComponent>()
			? playerEnt->GetComponent<InventoryComponent>()
			: playerEnt->AddComponent<InventoryComponent>();
		inv = RoguelikeState::GetInstance().inventory;
		ApplyWeaponLoadout(playerEnt, raidScene, inv);
		if (playerEnt->HasComponent<PlayerComponent>())
			playerEnt->GetComponent<PlayerComponent>().activeWeaponSlot = 0;
	}

	// Procedurally generate the map
	ProceduralMapSystem::GenerateMap(raidScene, newGs.raidDepth);
}

void GameStateSystem::ApplyWeaponLoadout(
	std::shared_ptr<Pengine::Entity> playerEntity,
	std::shared_ptr<Pengine::Scene> scene,
	const InventoryComponent& inv)
{
	auto camera = playerEntity->FindEntityInHierarchy("PlayerCamera");
	if (!camera) return;

	// Remove old weapon children
	std::vector<std::shared_ptr<Pengine::Entity>> toDelete;
	for (auto& weakChild : camera->GetChilds())
	{
		auto child = weakChild.lock();
		if (child && child->HasComponent<WeaponComponent>())
			toDelete.push_back(child);
	}
	for (auto& e : toDelete)
		scene->DeleteEntity(e);

	// Spawn equipped weapons from inventory
	for (int slot = 0; slot < InventoryComponent::kMaxWeaponSlots; ++slot)
	{
		if (!inv.weaponSlotOccupied[slot]) continue;
		const std::string& prefabPath = inv.weaponSlots[slot];
		if (prefabPath.empty()) continue;

		auto weaponEnt = Pengine::Serializer::DeserializePrefab(prefabPath, scene);
		if (weaponEnt)
		{
			weaponEnt->GetComponent<WeaponComponent>().slot = slot;
			camera->AddChild(weaponEnt, false);
		}
	}
}
