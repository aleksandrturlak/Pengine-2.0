#include "InteractableSystem.h"

#include "InteractableComponent.h"
#include "LootContainerComponent.h"
#include "ShopComponent.h"
#include "ShopSystem.h"
#include "InventoryComponent.h"
#include "InventorySystem.h"
#include "PlayerComponent.h"
#include "GameStateComponent.h"
#include "RoguelikeState.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/Input.h"
#include "Core/KeyCode.h"
#include "Core/WindowManager.h"
#include "Core/Window.h"
#include "Core/Logger.h"
#include "Core/Raycast.h"

#include "Components/Transform.h"

#include <glm/glm.hpp>
#include <algorithm>

std::string InteractableSystem::s_NearInteractableName;
std::string InteractableSystem::s_NearInteractableLabel;

void InteractableSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto playerEntity = scene->FindEntityByName("Player");
	if (!playerEntity || !playerEntity->HasComponent<PlayerComponent>()) return;

	auto& pc = playerEntity->GetComponent<PlayerComponent>();
	if (!pc.isAlive) return;

	auto& inv = playerEntity->HasComponent<InventoryComponent>()
		? playerEntity->GetComponent<InventoryComponent>()
		: playerEntity->AddComponent<InventoryComponent>();

	auto window = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
	if (!window) return;
	auto& input = Pengine::Input::GetInstance(window.get());

	glm::vec3 playerPos = playerEntity->GetComponent<Pengine::Transform>().GetPosition();

	// Clear last frame's nearest interactable
	s_NearInteractableName  = "";
	s_NearInteractableLabel = "";
	inv.nearInteractable    = "";

	// Find the closest non-loot interactable within proximity radius
	float bestDist = 1e9f;
	std::shared_ptr<Pengine::Entity> bestEntity;

	auto view = scene->GetRegistry().view<InteractableComponent>();
	for (auto handle : view)
	{
		auto& ic = view.get<InteractableComponent>(handle);
		if (ic.GetInteractType() == InteractableComponent::InteractType::LootContainer)
			continue; // handled via raycast below

		auto entity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
		if (!entity) continue;

		glm::vec3 pos  = entity->GetComponent<Pengine::Transform>().GetPosition();
		float dist = glm::length(playerPos - pos);
		if (dist < ic.radius && dist < bestDist)
		{
			bestDist   = dist;
			bestEntity = entity;
		}
	}

	// If a loot container UI is already open, use it directly (skip raycast to avoid BVH crash)
	{
		auto lootView = scene->GetRegistry().view<LootContainerComponent, InteractableComponent>();
		for (auto lh : lootView)
		{
			if (lootView.get<LootContainerComponent>(lh).isOpen)
			{
				auto lootEntity = scene->GetRegistry().get<Pengine::Transform>(lh).GetEntity();
				if (lootEntity)
					bestEntity = lootEntity;
				break;
			}
		}
	}

	// Raycast-based detection for loot containers: player must aim at the container
	auto cameraEntity = playerEntity->FindEntityInHierarchy("PlayerCamera");
	if (cameraEntity && !bestEntity)
	{
		auto& camTransform = cameraEntity->GetComponent<Pengine::Transform>();
		glm::vec3 rayOrigin = camTransform.GetPosition();
		glm::vec3 rayDir    = camTransform.GetForward();

		constexpr float kLootRayLength = 5.0f;
		auto hits = Pengine::Raycast::RaycastScene(scene, rayOrigin, rayDir, kLootRayLength);
		for (auto& [hit, hitEntity] : hits)
		{
			if (!hitEntity) continue;
			if (hitEntity->HasComponent<InteractableComponent>())
			{
				auto& ic = hitEntity->GetComponent<InteractableComponent>();
				if (ic.GetInteractType() == InteractableComponent::InteractType::LootContainer)
				{
					bestEntity = hitEntity;
					break;
				}
			}
		}
	}

	if (!bestEntity) return;

	auto& ic = bestEntity->GetComponent<InteractableComponent>();
	s_NearInteractableName  = bestEntity->GetName();
	s_NearInteractableLabel = ic.label;
	inv.nearInteractable    = bestEntity->GetName();

	if (!input.IsKeyPressed(Pengine::KeyCode::KEY_E)) return;

	// ── Handle interaction by type ────────────────────────────────────────
	switch (ic.GetInteractType())
	{
	case InteractableComponent::InteractType::Shop:
	{
		if (bestEntity->HasComponent<ShopComponent>())
		{
			auto& shop = bestEntity->GetComponent<ShopComponent>();
			shop.isOpen = !shop.isOpen;
			if (shop.isOpen)
			{
				// Close other UIs
				inv.inventoryOpen = false;
				window->ShowCursor();
			}
			else if (!inv.inventoryOpen)
				window->DisableCursor();
		}
		break;
	}
	case InteractableComponent::InteractType::Stash:
	{
		// Toggle stash UI — handled by UI script reading this entity name
		ic.isOpen = !ic.isOpen;
		if (ic.isOpen)
		{
			inv.inventoryOpen = false;
			window->ShowCursor();
		}
		else if (!inv.inventoryOpen)
			window->DisableCursor();
		break;
	}
	case InteractableComponent::InteractType::Extraction:
	{
		Pengine::Logger::Log("Player extracting...");
		RoguelikeState::GetInstance().pendingExtract = true;
		break;
	}
	case InteractableComponent::InteractType::RaidPortal:
	{
		Pengine::Logger::Log("Player entering raid portal...");
		RoguelikeState::GetInstance().pendingRaid = true;
		break;
	}
	case InteractableComponent::InteractType::LootContainer:
	{
		if (!ic.isLooted && bestEntity->HasComponent<LootContainerComponent>())
		{
			auto& lc = bestEntity->GetComponent<LootContainerComponent>();
			lc.isOpen = !lc.isOpen;
			if (lc.isOpen)
			{
				inv.inventoryOpen = false;
				window->ShowCursor();
			}
			else if (!inv.inventoryOpen)
				window->DisableCursor();
		}
		break;
	}
	}
}
