#pragma once

#include <string>
#include <glm/glm.hpp>
#include "Core/ReflectionSystem.h"

struct WeaponComponent
{
	PROPERTY(std::string, name, "")
	PROPERTY(float, damage, 25.0f)
	PROPERTY(float, fireRate, 0.3f)
	PROPERTY(int, slot, 0)
	PROPERTY(glm::vec3, restPosition, glm::vec3(0.0f, 0.0f, 0.0f))
	PROPERTY(float, kickUp, 0.03f)
	PROPERTY(float, kickBack, 0.05f)
	PROPERTY(float, cameraRecoilUp, 0.08f)
	PROPERTY(float, cameraRecoilSide, 0.03f)
	PROPERTY(float, recoilRecovery, 6.0f)
	PROPERTY(int, ammoTypeInt, 0)  // ItemComponent::AmmoType: 0=Pistol,1=Rifle,2=Shotgun
	PROPERTY(int, magazineSize, 12)
	PROPERTY(int, currentAmmo, 12)
	PROPERTY(float, reloadTime, 1.5f)
	PROPERTY(float, reloadTimer, 0.0f)
	PROPERTY(bool, isReloading, false)
	PROPERTY(float, swayTime, 0.0f)
	PROPERTY(int, pelletCount, 1)
	PROPERTY(float, spreadAngle, 0.0f)
	PROPERTY(float, physicalRecoilForce, 2.0f)
};
