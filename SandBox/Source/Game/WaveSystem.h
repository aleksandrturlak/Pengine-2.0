#pragma once

#include "ComponentSystems/ComponentSystem.h"

#include <map>
#include <functional>
#include <string>
#include <memory>

class WaveSystem : public Pengine::ComponentSystem
{
public:
	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return m_RemoveCallbacks; }

private:
	void SpawnWave(std::shared_ptr<Pengine::Entity> gameManager, std::shared_ptr<Pengine::Scene> scene, int wave, int count);

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>> m_RemoveCallbacks;
};
