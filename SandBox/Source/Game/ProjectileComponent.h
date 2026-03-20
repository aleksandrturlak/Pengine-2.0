#pragma once

#include "Core/ReflectionSystem.h"

struct ProjectileComponent
{
	PROPERTY(float, damage, 10.0f)
	PROPERTY(float, lifetime, 6.0f)
};
