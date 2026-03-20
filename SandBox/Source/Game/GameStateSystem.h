#pragma once

#include "ComponentSystems/ComponentSystem.h"
#include "InventoryComponent.h"
#include <map>
#include <functional>
#include <memory>

namespace Pengine
{
	class Entity;
	class Scene;
}

struct GameStateComponent;

class GameStateSystem : public Pengine::ComponentSystem
{
public:
	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return {}; }

private:
	void LoadHomeBase(std::shared_ptr<Pengine::Scene> currentScene);
	void LoadRaid(std::shared_ptr<Pengine::Scene> currentScene, GameStateComponent& gs);
	void ApplyWeaponLoadout(
		std::shared_ptr<Pengine::Entity> playerEntity,
		std::shared_ptr<Pengine::Scene> scene,
		const InventoryComponent& inv);
};
