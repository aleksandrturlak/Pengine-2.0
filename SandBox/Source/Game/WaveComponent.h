#pragma once

#include "Core/ReflectionSystem.h"

struct WaveComponent
{
	PROPERTY(int, currentWave, 0)
	PROPERTY(int, killCount, 0)
	PROPERTY(int, enemiesAlive, 0)
	PROPERTY(int, enemiesPerWave, 5)
	PROPERTY(int, maxWaves, 5)
	PROPERTY(float, timeBetweenWaves, 3.0f)
	PROPERTY(float, waveCountdown, 2.0f)
	PROPERTY(bool, waveInProgress, false)
	PROPERTY(bool, gameOver, false)
	PROPERTY(bool, playerWon, false)
};
