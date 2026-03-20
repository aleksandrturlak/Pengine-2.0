#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "Core/ReflectionSystem.h"

// Enemy patrol behavior — wander between waypoints until player spotted.
struct PatrolComponent
{
	enum class State { Patrol, Chase, Attack };

	std::vector<glm::vec3> waypoints;  // set at spawn time by ProceduralMapSystem

	PROPERTY(int,   currentWaypoint,  0)
	PROPERTY(int,   stateInt,         0)   // cast to State
	PROPERTY(float, patrolSpeed,      2.0f)
	PROPERTY(float, chaseSpeed,       4.5f)
	PROPERTY(float, detectionRange,   12.0f)
	PROPERTY(float, attackRange,      10.0f)
	PROPERTY(float, waypointTolerance,1.2f)
	PROPERTY(float, waitTimer,        0.0f)  // pause at each waypoint
	PROPERTY(float, waitDuration,     1.5f)

	// Strafe in Attack state
	PROPERTY(int,   strafeDir,         1)      // +1 or -1
	PROPERTY(float, strafeTimer,       1.5f)   // seconds until direction flip
	PROPERTY(float, strafeSpeed,       2.5f)   // set per type at spawn

	// Sniper: retreat if player is closer than this
	PROPERTY(float, retreatRange,      0.0f)

	// Sight-loss: return to Patrol if player out of detection range too long
	PROPERTY(float, sightLossTimer,    0.0f)   // runtime countdown
	PROPERTY(float, sightLossDuration, 3.0f)   // seconds before returning to Patrol

	State GetState() const { return static_cast<State>(stateInt); }
	void  SetState(State s) { stateInt = static_cast<int>(s); }
};
