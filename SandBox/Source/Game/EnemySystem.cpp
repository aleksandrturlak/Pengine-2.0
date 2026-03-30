#include "EnemySystem.h"

#include "EnemyComponent.h"
#include "PatrolComponent.h"
#include "PlayerComponent.h"
#include "ProjectileComponent.h"
#include "LootSystem.h"
#include "GameStateComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/Logger.h"

#include "Components/Transform.h"
#include "Components/RigidBody.h"
#include "Components/Renderer3D.h"
#include "Components/PointLight.h"

#include "Core/MeshManager.h"
#include "Core/MaterialManager.h"

#include "ComponentSystems/PhysicsSystem.h"
#include "ComponentSystems/AudioSystem.h"
#include "Components/AudioSource.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <cstdlib>

void EnemySystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto playerEntity = scene->FindEntityByName("Player");
	bool playerAlive = playerEntity && playerEntity->HasComponent<PlayerComponent>()
		&& playerEntity->GetComponent<PlayerComponent>().isAlive;

	glm::vec3 playerPos(0.0f);
	if (playerAlive)
		playerPos = playerEntity->GetComponent<Pengine::Transform>().GetPosition();

	auto physSys = scene->GetPhysicsSystem();
	auto& bodyInterface = physSys->GetInstance().GetBodyInterface();

	// Determine raid depth for loot generation
	int raidDepth = 1;
	auto gsEntity = scene->FindEntityByName("GameState");
	if (gsEntity && gsEntity->HasComponent<GameStateComponent>())
		raidDepth = gsEntity->GetComponent<GameStateComponent>().raidDepth;

	int enemiesAlive = 0;

	auto view = scene->GetRegistry().view<EnemyComponent>();
	for (auto handle : view)
	{
		EnemyComponent& enemy = view.get<EnemyComponent>(handle);

		if (!enemy.isAlive && !enemy.isDissolving) continue;

		// --- Dissolve animation ---
		if (enemy.isDissolving)
		{
			enemy.dissolveProgress += dt * enemy.dissolveSpeed;

			auto enemyEntity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
			if (!enemyEntity) continue;

			if (enemyEntity->HasComponent<Pengine::RigidBody>())
			{
				auto& rb = enemyEntity->GetComponent<Pengine::RigidBody>();
				if (rb.isValid)
				{
					physSys->SetLinearVelocity(rb, glm::vec3(0.0f));
					physSys->SetAngularVelocity(rb, glm::vec3(0.0f));
				}
			}

			if (enemyEntity->HasComponent<Pengine::Renderer3D>())
			{
				auto& r = enemyEntity->GetComponent<Pengine::Renderer3D>();
				if (r.material)
					r.material->WriteToBuffer("MaterialBuffer", "material.dissolveProgress", enemy.dissolveProgress);
			}

			if (enemy.dissolveProgress >= 1.0f)
				scene->DeleteEntity(enemyEntity);

			continue;
		}

		// --- Death: start dissolve + spawn loot stash ---
		if (enemy.health <= 0.0f)
		{
			enemy.isAlive      = false;
			enemy.isDissolving = true;

			auto enemyEntity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
			if (enemyEntity && enemyEntity->HasComponent<Pengine::Renderer3D>())
				enemyEntity->GetComponent<Pengine::Renderer3D>().castShadows = false;

			glm::vec3 deathPos = enemyEntity
				? enemyEntity->GetComponent<Pengine::Transform>().GetPosition()
				: glm::vec3(0.0f);

			// Update GameState enemy count
			if (gsEntity && gsEntity->HasComponent<GameStateComponent>())
			{
				auto& gs = gsEntity->GetComponent<GameStateComponent>();
				gs.enemiesRemaining = std::max(0, gs.enemiesRemaining - 1);
			}

			// Spawn loot stash with random chance (70%)
			if (std::rand() % 100 < 70)
				LootSystem::SpawnLootContainer(scene, deathPos, raidDepth, 1, 2, true);

			continue;
		}

		enemiesAlive++;

		// --- Shooting (when PatrolSystem put us in Attack state) ---
		auto enemyEntity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
		if (!enemyEntity) continue;

		bool inAttackState = enemyEntity->HasComponent<PatrolComponent>()
			&& enemyEntity->GetComponent<PatrolComponent>().GetState() == PatrolComponent::State::Attack;

		if (!inAttackState || !playerAlive) continue;

		// ── Scout melee ──────────────────────────────────────────────────────
		if (enemy.GetEnemyType() == EnemyComponent::EnemyType::Scout)
		{
			enemy.meleeCooldown -= dt;
			glm::vec3 enemyPos = enemyEntity->GetComponent<Pengine::Transform>().GetPosition();
			glm::vec3 diff = playerPos - enemyPos;
			float dist = glm::length(glm::vec3(diff.x, 0.0f, diff.z));
			if (enemy.meleeCooldown <= 0.0f && dist < enemy.meleeRange)
			{
				auto& pc = playerEntity->GetComponent<PlayerComponent>();
				pc.health       -= enemy.meleeDamage;
				pc.hitFlashTimer = 0.4f;
				if (pc.health <= 0.0f)
					pc.isAlive = false;
				enemy.meleeCooldown = enemy.meleeRate;
			}
			continue;  // Scout does not shoot
		}

		// ── Ranged shooting (Soldier / Heavy / Sniper) ───────────────────────
		auto& rb = enemyEntity->GetComponent<Pengine::RigidBody>();
		if (!rb.isValid) continue;

		enemy.shootCooldown -= dt;
		if (enemy.shootCooldown > 0.0f) continue;

		enemy.shootCooldown = enemy.shootRate;

		if (auto audioSys = std::dynamic_pointer_cast<Pengine::AudioSystem>(scene->GetComponentSystem("AudioSystem")))
		{
			if (!enemyEntity->HasComponent<Pengine::AudioSource>())
			{
				auto& src        = enemyEntity->AddComponent<Pengine::AudioSource>();
				src.filePath     = "Game/Assets/Sounds/LaserGunShot.mp3";
				src.loop         = false;
				src.spatialBlend = true;
				src.minDistance  = 2.0f;
				src.maxDistance  = 60.0f;
			}
			audioSys->Play(enemyEntity->GetComponent<Pengine::AudioSource>());
		}

		glm::vec3 enemyPos = enemyEntity->GetComponent<Pengine::Transform>().GetPosition();
		glm::vec3 eyePos   = enemyPos + glm::vec3(0.0f, 0.5f, 0.0f);
		glm::vec3 dir      = playerPos - eyePos;
		if (glm::length(dir) < 0.001f) continue;
		dir = glm::normalize(dir);

		glm::vec3 spawnPos = eyePos + dir * (rb.shape.sphere.radius + 0.3f);

		auto projEntity = scene->CreateEntity("EnemyProjectile");
		auto& t = projEntity->AddComponent<Pengine::Transform>(projEntity);
		t.Translate(spawnPos);
		t.Scale({ 0.15f, 0.15f, 0.15f });

		auto& r    = projEntity->AddComponent<Pengine::Renderer3D>();
		r.mesh     = Pengine::MeshManager::GetInstance().LoadMesh("Meshes/Sphere.mesh");
		r.material = Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/Projectile.mat");
		r.castShadows = false;

		auto& prb               = projEntity->AddComponent<Pengine::RigidBody>();
		prb.type                = Pengine::RigidBody::Type::Sphere;
		prb.shape.sphere.radius = 0.15f;
		prb.motionType          = Pengine::RigidBody::MotionType::Dynamic;
		prb.mass                = 0.1f;
		prb.friction            = 0.0f;
		prb.restitution         = 0.0f;
		prb.allowSleeping       = false;
		prb.linearVelocity      = dir * enemy.projectileSpeed;

		auto& proj    = projEntity->AddComponent<ProjectileComponent>();
		proj.damage   = enemy.projectileDamage;
		proj.lifetime = 6.0f;

		auto& pl = projEntity->AddComponent<Pengine::PointLight>();
		pl.radius = 10.0f;
		pl.intensity = 10.0f;
		pl.color = { 0.0f, 0.5f, 1.0f };
	}

	// Update GameState enemy count
	if (gsEntity && gsEntity->HasComponent<GameStateComponent>())
		gsEntity->GetComponent<GameStateComponent>().enemiesRemaining = enemiesAlive;
}
