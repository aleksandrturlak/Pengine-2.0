#include "SceneManager.h"

#include "MaterialManager.h"

#include "../ComponentSystems/SkeletalAnimatorSystem.h"
#include "../ComponentSystems/EntityAnimatorSystem.h"
#include "../ComponentSystems/AudioSystem.h"
#include "../ComponentSystems/UISystem.h"

using namespace Pengine;

SceneManager& SceneManager::GetInstance()
{
	static SceneManager sceneManager;
	return sceneManager;
}

std::shared_ptr<Scene> SceneManager::Create(const std::string& name, const std::string& tag)
{
	std::shared_ptr<Scene> scene = Scene::Create(name, tag);

	for (const auto& [name, system] : m_ComponentSystemsByName)
	{
		scene->SetComponentSystem(name, system);
	}

	m_ScenesByName[name] = scene;
	m_ScenesByTag[tag] = scene;
	
	return scene;
}

std::shared_ptr<Scene> SceneManager::GetSceneByName(const std::string& name) const
{
	if (const auto sceneByName = m_ScenesByName.find(name);
		sceneByName != m_ScenesByName.end())
	{
		return sceneByName->second;
	}

	return nullptr;
}

std::shared_ptr<Scene> SceneManager::GetSceneByTag(const std::string& tag) const
{
	if (const auto sceneByTag = m_ScenesByTag.find(tag);
		sceneByTag != m_ScenesByTag.end())
	{
		return sceneByTag->second;
	}

	return nullptr;
}

void SceneManager::Delete(const std::string& name)
{
	if (const auto sceneByName = m_ScenesByName.find(name);
		sceneByName != m_ScenesByName.end())
	{
		if (const auto sceneByTag = m_ScenesByTag.find(sceneByName->second->GetTag());
			sceneByTag != m_ScenesByTag.end())
		{
			m_ScenesByTag.erase(sceneByTag);
		}

		m_ScenesByName.erase(sceneByName);
	}
}

void SceneManager::Delete(std::shared_ptr<Scene>& scene)
{
	for (auto sceneIterator = m_ScenesByName.begin(); sceneIterator != m_ScenesByName.end(); sceneIterator++)
	{
		if (sceneIterator->second == scene)
		{
			m_ScenesByName.erase(sceneIterator);
			break;
		}
	}

	for (auto sceneIterator = m_ScenesByTag.begin(); sceneIterator != m_ScenesByTag.end(); sceneIterator++)
	{
		if (sceneIterator->second == scene)
		{
			m_ScenesByTag.erase(sceneIterator);
			break;
		}
	}

	scene = nullptr;
}

void SceneManager::ShutDown()
{
	m_ScenesByName.clear();
	m_ScenesByTag.clear();
	m_ComponentSystemsByName.clear();
}

SceneManager::SceneManager()
{
	SetComponentSystem<SkeletalAnimatorSystem>("SkeletalAnimatorSystem");
	SetComponentSystem<EntityAnimatorSystem>("EntityAnimatorSystem");
	SetComponentSystem<AudioSystem>("AudioSystem");
	SetComponentSystem<UISystem>("UISystem");
}
