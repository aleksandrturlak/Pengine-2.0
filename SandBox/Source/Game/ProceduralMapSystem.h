#pragma once

#include <memory>

namespace Pengine { class Scene; }

struct GameStateComponent;

// Free-function procedural map generator.
// Called by GameStateSystem when transitioning into a raid.
namespace ProceduralMapSystem
{
	void GenerateMap(std::shared_ptr<Pengine::Scene> scene, int raidDepth);
}
