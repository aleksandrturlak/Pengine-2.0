#pragma once

#include "Core/Application.h"

namespace Pengine
{
	class Material;
	class Mesh;
}

class GameApplication : public Pengine::Application
{
public:
	void OnPreStart() override;
	void OnStart() override;
	void OnClose() override;

private:
	void RegisterUIScripts();

	std::vector<std::shared_ptr<Pengine::Material>> m_CachedMaterials;
	std::vector<std::shared_ptr<Pengine::Mesh>>     m_CachedMeshes;
};
