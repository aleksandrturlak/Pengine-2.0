#pragma once

#include "ComponentSystems/ComponentSystem.h"
#include <map>
#include <functional>
#include <memory>

namespace Pengine { class Entity; class Scene; }

class PatrolSystem : public Pengine::ComponentSystem
{
public:
	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return {}; }
};
