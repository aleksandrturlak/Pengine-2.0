#include "PatrolSystem.h"

#include "PatrolComponent.h"
#include "EnemyComponent.h"
#include "PlayerComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"

#include "Components/Transform.h"
#include "Components/RigidBody.h"

#include "ComponentSystems/PhysicsSystem.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <cstdlib>
#include <algorithm>

void PatrolSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto playerEntity = scene->FindEntityByName("Player");
	glm::vec3 playerPos(0.0f);
	bool playerAlive = false;

	if (playerEntity && playerEntity->HasComponent<PlayerComponent>())
	{
		playerAlive = playerEntity->GetComponent<PlayerComponent>().isAlive;
		playerPos   = playerEntity->GetComponent<Pengine::Transform>().GetPosition();
	}

	auto physSys = scene->GetPhysicsSystem();
	auto& bodyInterface = physSys->GetInstance().GetBodyInterface();

	auto view = scene->GetRegistry().view<PatrolComponent, EnemyComponent>();
	for (auto handle : view)
	{
		auto& patrol = view.get<PatrolComponent>(handle);
		auto& enemy  = view.get<EnemyComponent>(handle);

		if (!enemy.isAlive || enemy.isDissolving) continue;

		auto entity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
		if (!entity) continue;
		if (!entity->HasComponent<Pengine::RigidBody>()) continue;

		auto& rb = entity->GetComponent<Pengine::RigidBody>();
		if (!rb.isValid) continue;

		auto& transform = entity->GetComponent<Pengine::Transform>();
		glm::vec3 pos   = transform.GetPosition();

		// Keep upright
		physSys->SetAngularVelocity(rb, glm::vec3(0.0f));
		float currentYVel = bodyInterface.GetLinearVelocity(rb.id).GetY();

		// Face movement/player direction helper
		auto faceDirection = [&](glm::vec3 dir)
		{
			glm::vec3 flat = glm::vec3(dir.x, 0.0f, dir.z);
			if (glm::length(flat) > 0.001f)
			{
				flat = glm::normalize(flat);
				float yaw = std::atan2(flat.x, flat.z);
				glm::quat q = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
				bodyInterface.SetRotation(rb.id, Pengine::GlmQuatToJoltQuat(q), JPH::EActivation::Activate);
			}
		};

		// ── State transitions ────────────────────────────────────────────
		if (playerAlive)
		{
			float distToPlayer = glm::length(playerPos - pos);

			switch (patrol.GetState())
			{
			case PatrolComponent::State::Patrol:
				if (distToPlayer <= patrol.detectionRange)
				{
					patrol.SetState(PatrolComponent::State::Chase);
					patrol.sightLossTimer = 0.0f;
				}
				break;
			case PatrolComponent::State::Chase:
				if (distToPlayer <= patrol.attackRange)
				{
					patrol.SetState(PatrolComponent::State::Attack);
					patrol.sightLossTimer = 0.0f;
				}
				else if (distToPlayer > patrol.detectionRange)
				{
					// Player left detection range — start sight-loss countdown
					patrol.sightLossTimer += dt;
					if (patrol.sightLossTimer >= patrol.sightLossDuration)
					{
						patrol.SetState(PatrolComponent::State::Patrol);
						patrol.sightLossTimer = 0.0f;
					}
				}
				else
				{
					patrol.sightLossTimer = 0.0f;
				}
				break;
			case PatrolComponent::State::Attack:
				if (distToPlayer > patrol.attackRange * 1.2f)
				{
					patrol.SetState(PatrolComponent::State::Chase);
					patrol.sightLossTimer = 0.0f;
				}
				break;
			}
		}
		else
		{
			patrol.SetState(PatrolComponent::State::Patrol);
			patrol.sightLossTimer = 0.0f;
		}

		// ── Behaviour per state ─────────────────────────────────────────
		switch (patrol.GetState())
		{
		case PatrolComponent::State::Patrol:
		{
			if (patrol.waypoints.empty())
			{
				physSys->SetLinearVelocity(rb, glm::vec3(0.0f, currentYVel, 0.0f));
				break;
			}

			// Wait at current waypoint
			if (patrol.waitTimer > 0.0f)
			{
				patrol.waitTimer -= dt;
				physSys->SetLinearVelocity(rb, glm::vec3(0.0f, currentYVel, 0.0f));
				break;
			}

			glm::vec3 target = patrol.waypoints[patrol.currentWaypoint % (int)patrol.waypoints.size()];
			glm::vec3 toTarget = target - pos;
			float dist = glm::length(glm::vec3(toTarget.x, 0.0f, toTarget.z));

			if (dist < patrol.waypointTolerance)
			{
				// Reached waypoint — wait then advance
				patrol.waitTimer = patrol.waitDuration;
				patrol.currentWaypoint = (patrol.currentWaypoint + 1) % (int)patrol.waypoints.size();
				physSys->SetLinearVelocity(rb, glm::vec3(0.0f, currentYVel, 0.0f));
			}
			else
			{
				glm::vec3 dir = glm::normalize(glm::vec3(toTarget.x, 0.0f, toTarget.z));
				faceDirection(dir);
				glm::vec3 vel = dir * patrol.patrolSpeed;
				vel.y = currentYVel;
				physSys->SetLinearVelocity(rb, vel);
			}
			break;
		}
		case PatrolComponent::State::Chase:
		{
			glm::vec3 toPlayer = playerPos - pos;
			glm::vec3 dir = glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
			faceDirection(dir);
			glm::vec3 vel = dir * patrol.chaseSpeed;
			vel.y = currentYVel;
			physSys->SetLinearVelocity(rb, vel);
			break;
		}
		case PatrolComponent::State::Attack:
		{
			glm::vec3 toPlayer = playerPos - pos;
			faceDirection(toPlayer);  // always face player

			// Scout keeps rushing until within melee range, then stops
			if (enemy.GetEnemyType() == EnemyComponent::EnemyType::Scout)
			{
				if (glm::length(glm::vec3(toPlayer.x, 0.0f, toPlayer.z)) > enemy.meleeRange)
				{
					glm::vec3 dir = glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
					glm::vec3 vel = dir * patrol.chaseSpeed;
					vel.y = currentYVel;
					physSys->SetLinearVelocity(rb, vel);
				}
				else
				{
					physSys->SetLinearVelocity(rb, glm::vec3(0.0f, currentYVel, 0.0f));
				}
				break;
			}

			float distToPlayer = glm::length(toPlayer);

			// Sniper retreat: back away if player is too close
			if (patrol.retreatRange > 0.0f && distToPlayer < patrol.retreatRange)
			{
				glm::vec3 awayDir = -glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
				glm::vec3 vel = awayDir * patrol.strafeSpeed;
				vel.y = currentYVel;
				physSys->SetLinearVelocity(rb, vel);
				break;
			}

			// Strafe timer — flip direction every 1.0–2.5 seconds
			patrol.strafeTimer -= dt;
			if (patrol.strafeTimer <= 0.0f)
			{
				patrol.strafeDir   = -patrol.strafeDir;
				patrol.strafeTimer = 1.0f + static_cast<float>(std::rand() % 151) / 100.0f;
			}

			// Perpendicular axis to player direction
			glm::vec3 flatToPlayer = glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
			glm::vec3 strafeAxis   = glm::cross(flatToPlayer, glm::vec3(0.0f, 1.0f, 0.0f));
			if (glm::length(strafeAxis) > 0.001f)
				strafeAxis = glm::normalize(strafeAxis);

			glm::vec3 vel = strafeAxis * (patrol.strafeSpeed * static_cast<float>(patrol.strafeDir));
			vel.y = currentYVel;
			physSys->SetLinearVelocity(rb, vel);
			break;
		}
		}
	}
}
