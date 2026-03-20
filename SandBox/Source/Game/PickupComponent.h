#pragma once

#include "Core/ReflectionSystem.h"

struct PickupComponent
{
	enum class Type { Health, Ammo };

	PROPERTY(Type, type, Type::Health)
	PROPERTY(float, value, 25.0f)
	PROPERTY(int, ammoTypeInt, 0)   // 0=Pistol,1=Rifle,2=Shotgun (used when type==Ammo)
	PROPERTY(int, ammoCount, 15)    // how many rounds this pickup grants
	PROPERTY(float, pickupRadius, 1.5f)
	PROPERTY(float, spinAngle, 0.0f)
	PROPERTY(float, spinSpeed, 2.0f)
	PROPERTY(float, bobTime, 0.0f)
	PROPERTY(float, bobSpeed, 2.0f)
	PROPERTY(float, bobAmplitude, 0.15f)
	PROPERTY(float, baseY, 0.0f)
};
