#pragma once

#include "../Core/Core.h"

#include "ComponentSystem.h"

#include <clay/clay.hpp>

namespace Pengine
{

	class PENGINE_API UISystem : public ComponentSystem
	{
	public:
		using ScriptFn = std::function<std::vector<clay::RenderCommand>(class Canvas*, clay::Context*, std::shared_ptr<class Entity>)>;

		static std::unordered_map<std::string, ScriptFn>& Scripts();

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
