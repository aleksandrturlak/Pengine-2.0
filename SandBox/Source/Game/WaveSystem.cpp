#include "WaveSystem.h"

#include "WaveComponent.h"
#include "PlayerComponent.h"
#include "EnemyComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/MeshManager.h"
#include "Core/MaterialManager.h"
#include "Graphics/Material.h"

#include <cstdlib>

#include "Components/Transform.h"
#include "Components/RigidBody.h"
#include "Components/Renderer3D.h"

#include <glm/glm.hpp>

void WaveSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto gmEntity = scene->FindEntityByName("GameManager");
	if (!gmEntity || !gmEntity->HasComponent<WaveComponent>())
		return;

	WaveComponent& wc = gmEntity->GetComponent<WaveComponent>();

	auto playerEntity = scene->FindEntityByName("Player");
	if (playerEntity && playerEntity->HasComponent<PlayerComponent>())
	{
		if (!playerEntity->GetComponent<PlayerComponent>().isAlive)
			wc.gameOver = true;
	}

	if (wc.gameOver)
		return;

	if (!wc.waveInProgress)
	{
		wc.waveCountdown -= dt;
		if (wc.waveCountdown > 0.0f)
			return;

		if (wc.currentWave >= wc.maxWaves)
		{
			wc.playerWon = true;
			wc.gameOver  = true;
			return;
		}

		wc.currentWave++;
		int count = wc.enemiesPerWave + (wc.currentWave - 1) * 2;
		SpawnWave(gmEntity, scene, wc.currentWave, count);
		wc.enemiesAlive   = count;
		wc.waveInProgress = true;
	}
	else
	{
		// Count live enemies
		int alive = 0;
		auto view = scene->GetRegistry().view<EnemyComponent>();
		for (auto handle : view)
		{
			if (view.get<EnemyComponent>(handle).isAlive)
				alive++;
		}

		if (alive == 0)
		{
			wc.waveInProgress = false;
			wc.waveCountdown  = wc.timeBetweenWaves;
		}
	}
}

void WaveSystem::SpawnWave(std::shared_ptr<Pengine::Entity> gameManager, std::shared_ptr<Pengine::Scene> scene, int wave, int count)
{
	const auto& spawnPoints = gameManager->GetChilds();
	if (spawnPoints.empty())
	{
		return;
	}
	
	auto sphereMesh        = Pengine::MeshManager::GetInstance().LoadMesh("Meshes/Sphere.mesh");
	auto dissolveMatTemplate = Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/EnemyDissolve.mat");

	for (int i = 0; i < count; ++i)
	{
		auto spawnPoint = spawnPoints[i % spawnPoints.size()];

		auto enemy = scene->CreateEntity("Enemy");
		auto& t = enemy->AddComponent<Pengine::Transform>(enemy);
		t.Translate(spawnPoint.lock()->GetComponent<Pengine::Transform>().GetPosition());

		// Clone the template so each enemy has its own material buffer (unique dissolve)
		auto clonedMat = Pengine::Material::Clone(
			"EnemyDissolve_" + std::to_string(i),
			{},
			dissolveMatTemplate);
		float seed = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		clonedMat->WriteToBuffer("MaterialBuffer", "material.dissolveNoiseSeed", seed);

		auto& r = enemy->AddComponent<Pengine::Renderer3D>();
		r.mesh        = sphereMesh;
		r.material    = clonedMat;
		r.castShadows = true;

		auto& rb = enemy->AddComponent<Pengine::RigidBody>();
		rb.type                  = Pengine::RigidBody::Type::Sphere;
		rb.shape.sphere.radius   = 1.0f;
		rb.motionType            = Pengine::RigidBody::MotionType::Dynamic;
		rb.mass                  = 80.0f;
		rb.friction              = 0.3f;
		rb.restitution           = 0.0f;
		rb.allowSleeping         = false;

		auto& ec      = enemy->AddComponent<EnemyComponent>();
		ec.health     = 50.0f + (wave - 1) * 10.0f;
		ec.moveSpeed  = 3.0f  + (wave - 1) * 0.3f;
	}
}
