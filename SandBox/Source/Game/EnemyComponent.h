#pragma once

#include "Core/ReflectionSystem.h"

struct EnemyComponent
{
	PROPERTY(float, health, 50.0f)
	PROPERTY(float, moveSpeed, 3.0f)
	PROPERTY(float, shootRange, 15.0f)
	PROPERTY(float, shootCooldown, 0.0f)
	PROPERTY(float, shootRate, 1.5f)
	PROPERTY(float, projectileDamage, 5.0f)
	PROPERTY(float, projectileSpeed, 25.0f)

	// Enemy type
	enum class EnemyType { Scout = 0, Soldier = 1, Heavy = 2, Sniper = 3 };
	PROPERTY(int, enemyTypeInt, 1)  // default Soldier
	EnemyType GetEnemyType() const { return static_cast<EnemyType>(enemyTypeInt); }
	void      SetEnemyType(EnemyType t) { enemyTypeInt = static_cast<int>(t); }

	// Scout melee
	PROPERTY(float, meleeDamage,   15.0f)
	PROPERTY(float, meleeRange,     1.5f)
	PROPERTY(float, meleeCooldown,  0.0f)  // runtime countdown
	PROPERTY(float, meleeRate,      0.8f)  // seconds between hits

	PROPERTY(bool, isAlive, true)
	PROPERTY(bool, isDissolving, false)
	PROPERTY(float, dissolveProgress, 0.0f)
	PROPERTY(float, dissolveSpeed, 0.8f)
};
