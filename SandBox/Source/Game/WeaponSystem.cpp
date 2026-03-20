#include "WeaponSystem.h"

#include "PlayerComponent.h"
#include "WeaponComponent.h"
#include "InventoryComponent.h"
#include "GameStateComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/Input.h"
#include "Core/KeyCode.h"
#include "Core/WindowManager.h"
#include "Core/Window.h"

#include "Components/Transform.h"

void WeaponSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto window = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
	if (!window)
		return;

	auto& input = Pengine::Input::GetInstance(window.get());

	// Freeze during game over or inventory open
	auto gsEntity = scene->FindEntityByName("GameState");
	bool gameOver = gsEntity && gsEntity->HasComponent<GameStateComponent>()
		&& gsEntity->GetComponent<GameStateComponent>().GetPhase() == GameStateComponent::Phase::GameOver;

	auto view = scene->GetRegistry().view<PlayerComponent>();
	for (auto handle : view)
	{
		PlayerComponent& player = view.get<PlayerComponent>(handle);

		if (!player.isAlive || gameOver)
			continue;

		// Don't switch weapons while inventory is open
		auto entity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
		if (!entity) continue;
		if (entity->HasComponent<InventoryComponent>()
			&& entity->GetComponent<InventoryComponent>().inventoryOpen)
			continue;

		auto camera = entity->FindEntityInHierarchy("PlayerCamera");
		if (!camera)
			continue;

		// Detect slot key press (only 2 slots)
		int newSlot = player.activeWeaponSlot;
		if (input.IsKeyPressed(Pengine::KeyCode::KEY_1)) newSlot = 0;
		if (input.IsKeyPressed(Pengine::KeyCode::KEY_2)) newSlot = 1;

		if (newSlot != player.activeWeaponSlot || !player.weaponsInitialized)
		{
			player.activeWeaponSlot   = newSlot;
			player.weaponsInitialized = true;
		}

		// Always sync visibility so newly equipped weapons are hidden when not active
		std::shared_ptr<Pengine::Entity> activeWeapon;
		for (auto& weakChild : camera->GetChilds())
		{
			auto child = weakChild.lock();
			if (!child) continue;
			if (child->HasComponent<WeaponComponent>())
			{
				auto& wc = child->GetComponent<WeaponComponent>();
				bool active = wc.slot == player.activeWeaponSlot;
				child->SetEnabled(active);
				if (active)
					activeWeapon = child;
			}
		}

		// Toggle flashlight on the active weapon with L
		if (input.IsKeyPressed(Pengine::KeyCode::KEY_L) && activeWeapon)
		{
			auto flashlight = activeWeapon->FindEntityInHierarchy("FlashLight");
			if (flashlight)
				flashlight->SetEnabled(!flashlight->IsEnabled());
		}
	}
}
