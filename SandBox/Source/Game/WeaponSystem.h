#pragma once

#include "ComponentSystems/ComponentSystem.h"

#include <map>
#include <string>
#include <functional>
#include <memory>

class WeaponSystem : public Pengine::ComponentSystem
{
public:
	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return m_RemoveCallbacks; }

private:
	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>> m_RemoveCallbacks;
};
