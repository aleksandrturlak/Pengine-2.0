#pragma once

#include "../Core/Core.h"

#include "ComponentSystem.h"

namespace Pengine
{

	class PENGINE_API UISystem : public ComponentSystem
	{
	public:
		UISystem();
		virtual ~UISystem() override = default;

		virtual void OnUpdate(const float deltaTime, std::shared_ptr<class Scene> scene) override;

		virtual std::map<std::string, std::function<void(std::shared_ptr<class Entity>)>> GetRemoveCallbacks() override
		{
			return m_RemoveCallbacks;
		}

	private:
		std::map<std::string, std::function<void(std::shared_ptr<class Entity>)>> m_RemoveCallbacks;
	};

}
