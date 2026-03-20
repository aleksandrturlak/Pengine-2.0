#pragma once

#include "ComponentSystems/ComponentSystem.h"

#include <map>
#include <string>
#include <functional>
#include <memory>
#include <vector>

class ProjectileSystem : public Pengine::ComponentSystem
{
public:
	ProjectileSystem();
	~ProjectileSystem();

	void OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene) override;

	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>>
		GetRemoveCallbacks() override { return m_RemoveCallbacks; }

private:
	struct PendingHit
	{
		std::shared_ptr<Pengine::Entity> projectile;
		float damage   = 0.0f;
		bool  hitPlayer = false;
	};

	std::vector<PendingHit> m_PendingHits;
	std::map<std::string, std::function<void(std::shared_ptr<Pengine::Entity>)>> m_RemoveCallbacks;
};
