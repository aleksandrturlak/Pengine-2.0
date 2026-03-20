#pragma once

#include "Core/ReflectionSystem.h"

struct PlayerComponent
{
	PROPERTY(float, health, 100.0f)
	PROPERTY(float, maxHealth, 100.0f)
	PROPERTY(float, moveSpeed, 6.0f)
	PROPERTY(float, jumpSpeed, 7.0f)
	PROPERTY(float, mouseSensitivity, 1.0f)
	PROPERTY(float, shootCooldown, 0.0f)
	PROPERTY(float, yawAngle, 0.0f)
	PROPERTY(float, pitchAngle, 0.0f)
	PROPERTY(int, activeWeaponSlot, 0)
	PROPERTY(bool, weaponsInitialized, false)
	PROPERTY(bool, isAlive, true)
	PROPERTY(bool, cursorDisabled, false)
	PROPERTY(bool, gameOverHandled, false)
	PROPERTY(float, hitFlashTimer, 0.0f)
	PROPERTY(float, recoilTimer, 0.0f)
	PROPERTY(float, recoilDuration, 0.18f)
	PROPERTY(float, recoilPitch, 0.0f)
	PROPERTY(float, recoilYaw, 0.0f)
	PROPERTY(float, recoilRecovery, 6.0f)
	PROPERTY(float, muzzleFlashTimer, 0.0f)
	PROPERTY(float, muzzleFlashDuration, 0.06f)
};
