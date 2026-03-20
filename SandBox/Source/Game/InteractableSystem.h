#pragma once

#include "ComponentSystems/ComponentSystem.h"
#include <map>
#include <functional>
#include <memory>
#include <string>

namespace Pengine { class Entity; class Scene; }

class InteractableSystem : public Pengine::ComponentSystem
{
public:
	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return {}; }

	// The name of the interactable the player is currently near (or empty)
	// Written by OnUpdate, read by UI scripts
	static std::string s_NearInteractableName;
	static std::string s_NearInteractableLabel;
};
