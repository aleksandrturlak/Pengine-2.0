#pragma once

#include "Core/ReflectionSystem.h"

// Global game state — one entity in whatever scene is active holds this.
// GameStateSystem reads/writes it to manage scene transitions.
struct GameStateComponent
{
	enum class Phase { HomeBase, LoadingRaid, InRaid, LoadingHome, GameOver };

	PROPERTY(int,   phaseInt,         0)   // cast to Phase
	PROPERTY(int,   raidDepth,        0)   // how many raids completed
	PROPERTY(int,   enemiesRemaining, 0)
	PROPERTY(bool,  playerDied,       false)
	PROPERTY(float, transitionTimer,  0.0f)
	PROPERTY(bool,  extractionReady,  false)  // at least one extraction portal nearby
	PROPERTY(bool,  inventoryInjected,false)   // true after first-frame inventory sync

	Phase GetPhase() const { return static_cast<Phase>(phaseInt); }
	void  SetPhase(Phase p) { phaseInt = static_cast<int>(p); }
};
