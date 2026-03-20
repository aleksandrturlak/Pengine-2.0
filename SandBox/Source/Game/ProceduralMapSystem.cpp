#include "ProceduralMapSystem.h"

#include "EnemyComponent.h"
#include "PatrolComponent.h"
#include "InteractableComponent.h"
#include "LootSystem.h"
#include "GameStateComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/MeshManager.h"
#include "Core/MaterialManager.h"
#include "Core/Logger.h"
#include "Graphics/Material.h"

#include "Components/Transform.h"
#include "Components/RigidBody.h"
#include "Components/Renderer3D.h"
#include "ComponentSystems/PhysicsSystem.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

namespace
{
	// ── Map grid constants ────────────────────────────────────────────────────
	constexpr int   kGridSize    = 9;   // kGridSize x kGridSize room grid
	constexpr float kRoomSize    = 18.0f;
	constexpr float kWallH       = 4.0f;
	constexpr float kWallThick   = 0.6f;
	constexpr float kFloorThick  = 0.3f;
	constexpr float kCorridorW   = 4.0f;

	struct Room
	{
		bool    active     = false;
		bool    connected  = false;
		bool    hasEnemy   = false;
		bool    hasLoot    = false;
		bool    isExtract  = false;
		glm::vec2 center;
	};

	using Grid = std::array<std::array<Room, kGridSize>, kGridSize>;

	glm::vec3 RoomWorldPos(int gx, int gy)
	{
		return glm::vec3(gx * kRoomSize, 0.0f, gy * kRoomSize);
	}

	// ── Entity builders ───────────────────────────────────────────────────────

	void SpawnFloor(std::shared_ptr<Pengine::Scene> scene,
	                std::shared_ptr<Pengine::Mesh>  cubeMesh,
	                std::shared_ptr<Pengine::Material> mat,
	                glm::vec3 center, float sx, float sz)
	{
		auto e  = scene->CreateEntity("Floor");
		auto& t = e->AddComponent<Pengine::Transform>(e);
		t.Translate(center - glm::vec3(0.0f, kFloorThick * 0.5f, 0.0f));
		t.Scale({ sx * 0.5f, kFloorThick * 0.5f, sz * 0.5f });

		auto& r   = e->AddComponent<Pengine::Renderer3D>();
		r.mesh     = cubeMesh;
		r.material = mat;
		r.castShadows = false;

		auto& rb = e->AddComponent<Pengine::RigidBody>();
		rb.type   = Pengine::RigidBody::Type::Box;
		rb.shape.box.halfExtents = { sx * 0.5f, kFloorThick * 0.5f, sz * 0.5f };
		rb.motionType = Pengine::RigidBody::MotionType::Static;
	}

	void SpawnWall(std::shared_ptr<Pengine::Scene> scene,
	               std::shared_ptr<Pengine::Mesh>  cubeMesh,
	               std::shared_ptr<Pengine::Material> mat,
	               glm::vec3 center, float sx, float sy, float sz)
	{
		auto e  = scene->CreateEntity("Wall");
		auto& t = e->AddComponent<Pengine::Transform>(e);
		t.Translate(center);
		t.Scale({ sx * 0.5f, sy * 0.5f, sz * 0.5f });

		auto& r   = e->AddComponent<Pengine::Renderer3D>();
		r.mesh     = cubeMesh;
		r.material = mat;

		auto& rb = e->AddComponent<Pengine::RigidBody>();
		rb.type   = Pengine::RigidBody::Type::Box;
		rb.shape.box.halfExtents = { sx * 0.5f, sy * 0.5f, sz * 0.5f };
		rb.motionType = Pengine::RigidBody::MotionType::Static;
	}

	EnemyComponent::EnemyType RollEnemyType(int raidDepth)
	{
		int roll = std::rand() % 100;
		if (raidDepth <= 1)
		{
			// Scout 40%, Soldier 40%, Heavy 20%, Sniper 0%
			if (roll < 40) return EnemyComponent::EnemyType::Scout;
			if (roll < 80) return EnemyComponent::EnemyType::Soldier;
			return EnemyComponent::EnemyType::Heavy;
		}
		else if (raidDepth == 2)
		{
			// Scout 30%, Soldier 30%, Heavy 25%, Sniper 15%
			if (roll < 30) return EnemyComponent::EnemyType::Scout;
			if (roll < 60) return EnemyComponent::EnemyType::Soldier;
			if (roll < 85) return EnemyComponent::EnemyType::Heavy;
			return EnemyComponent::EnemyType::Sniper;
		}
		else
		{
			// Scout 20%, Soldier 25%, Heavy 25%, Sniper 30%
			if (roll < 20) return EnemyComponent::EnemyType::Scout;
			if (roll < 45) return EnemyComponent::EnemyType::Soldier;
			if (roll < 70) return EnemyComponent::EnemyType::Heavy;
			return EnemyComponent::EnemyType::Sniper;
		}
	}

	void SpawnEnemy(std::shared_ptr<Pengine::Scene> scene,
	                std::shared_ptr<Pengine::Mesh> sphereMesh,
	                std::shared_ptr<Pengine::Material> dissolveMatTemplate,
	                glm::vec3 pos, int raidDepth, int index,
	                std::vector<glm::vec3> waypoints,
	                EnemyComponent::EnemyType type)
	{
		const int d = raidDepth - 1;

		auto enemy = scene->CreateEntity("Enemy");
		auto& t    = enemy->AddComponent<Pengine::Transform>(enemy);
		t.Translate(pos);

		auto clonedMat = Pengine::Material::Clone(
			"EnemyDissolve_" + std::to_string(index), {}, dissolveMatTemplate);
		float seed = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		clonedMat->WriteToBuffer("MaterialBuffer", "material.dissolveNoiseSeed", seed);

		auto& r   = enemy->AddComponent<Pengine::Renderer3D>();
		r.mesh        = sphereMesh;
		r.material    = clonedMat;
		r.castShadows = true;

		auto& rb          = enemy->AddComponent<Pengine::RigidBody>();
		rb.type           = Pengine::RigidBody::Type::Sphere;
		rb.motionType     = Pengine::RigidBody::MotionType::Dynamic;
		rb.mass           = 80.0f;
		rb.friction       = 0.3f;
		rb.restitution    = 0.0f;
		rb.allowSleeping  = false;

		auto& ec = enemy->AddComponent<EnemyComponent>();
		auto& pc = enemy->AddComponent<PatrolComponent>();
		ec.SetEnemyType(type);
		pc.waypoints = waypoints;

		switch (type)
		{
		case EnemyComponent::EnemyType::Scout:
			ec.health          = 30.0f + d * 8.0f;
			ec.meleeDamage     = 15.0f;
			ec.meleeRange      = 1.5f;
			ec.meleeRate       = 0.8f;
			pc.patrolSpeed     = 3.0f;
			pc.chaseSpeed      = 7.0f + d * 0.5f;
			pc.detectionRange  = 12.0f + d * 1.5f;
			pc.attackRange     = ec.meleeRange + 0.5f;
			pc.strafeSpeed     = 0.0f;
			pc.retreatRange    = 0.0f;
			rb.shape.sphere.radius = 0.7f;
			t.Scale({ 0.7f, 0.7f, 0.7f });
			{ glm::vec4 col(0.1f, 0.9f, 0.1f, 1.0f); clonedMat->WriteToBuffer("MaterialBuffer", "material.albedoColor", col); } // green
			break;

		case EnemyComponent::EnemyType::Soldier:
			ec.health          = 60.0f  + d * 15.0f;
			ec.shootRange      = 14.0f;
			ec.shootRate       = 1.5f;
			ec.projectileDamage = 8.0f;
			ec.projectileSpeed  = 25.0f;
			pc.patrolSpeed     = 2.0f;
			pc.chaseSpeed      = 4.0f + d * 0.4f;
			pc.detectionRange  = 12.0f + d * 1.5f;
			pc.attackRange     = ec.shootRange;
			pc.strafeSpeed     = 2.5f;
			pc.retreatRange    = 0.0f;
			rb.shape.sphere.radius = 1.0f;
			t.Scale({ 1.0f, 1.0f, 1.0f });
			{ glm::vec4 col(1.0f, 0.2f, 0.2f, 1.0f); clonedMat->WriteToBuffer("MaterialBuffer", "material.albedoColor", col); } // red
			break;

		case EnemyComponent::EnemyType::Heavy:
			ec.health          = 150.0f + d * 30.0f;
			ec.shootRange      = 10.0f;
			ec.shootRate       = 0.8f;
			ec.projectileDamage = 20.0f;
			ec.projectileSpeed  = 18.0f;
			pc.patrolSpeed     = 1.5f;
			pc.chaseSpeed      = 2.5f + d * 0.2f;
			pc.detectionRange  = 10.0f + d * 1.0f;
			pc.attackRange     = ec.shootRange;
			pc.strafeSpeed     = 1.2f;
			pc.retreatRange    = 0.0f;
			rb.shape.sphere.radius = 1.5f;
			t.Scale({ 1.5f, 1.5f, 1.5f });
			{ glm::vec4 col(0.6f, 0.1f, 0.9f, 1.0f); clonedMat->WriteToBuffer("MaterialBuffer", "material.albedoColor", col); } // purple
			break;

		case EnemyComponent::EnemyType::Sniper:
			ec.health          = 40.0f + d * 10.0f;
			ec.shootRange      = 28.0f;
			ec.shootRate       = 3.0f;
			ec.projectileDamage = 25.0f;
			ec.projectileSpeed  = 40.0f;
			pc.patrolSpeed     = 1.5f;
			pc.chaseSpeed      = 3.0f + d * 0.3f;
			pc.detectionRange  = 20.0f + d * 2.0f;
			pc.attackRange     = ec.shootRange;
			pc.strafeSpeed     = 2.0f;
			pc.retreatRange    = 12.0f;
			rb.shape.sphere.radius = 0.9f;
			t.Scale({ 0.9f, 0.9f, 0.9f });
			{ glm::vec4 col(0.1f, 0.4f, 1.0f, 1.0f); clonedMat->WriteToBuffer("MaterialBuffer", "material.albedoColor", col); } // blue
			break;
		}

		// Randomise initial strafe state so enemies don't all flip simultaneously
		pc.strafeDir   = (std::rand() % 2 == 0) ? 1 : -1;
		pc.strafeTimer = 1.0f + static_cast<float>(std::rand() % 151) / 100.0f;
	}

	void SpawnExtractionPoint(std::shared_ptr<Pengine::Scene> scene,
	                          std::shared_ptr<Pengine::Mesh>  cubeMesh,
	                          std::shared_ptr<Pengine::Material> mat,
	                          glm::vec3 pos)
	{
		auto e  = scene->CreateEntity("ExtractionPoint");
		auto& t = e->AddComponent<Pengine::Transform>(e);
		t.Translate(pos + glm::vec3(0.0f, 0.1f, 0.0f));
		t.Scale({ 1.5f, 0.1f, 1.5f });

		auto& r   = e->AddComponent<Pengine::Renderer3D>();
		r.mesh     = cubeMesh;
		r.material = mat;
		r.castShadows = false;

		auto& ic           = e->AddComponent<InteractableComponent>();
		ic.interactTypeInt = static_cast<int>(InteractableComponent::InteractType::Extraction);
		ic.radius          = 3.5f;
		ic.label           = "[E] Extract";
	}

	// ── Simple BSP-like map generation ───────────────────────────────────────

	void CarveRooms(Grid& grid, int raidDepth)
	{
		// Determine how many rooms to fill based on depth
		int targetRooms = std::min(12 + raidDepth * 3, kGridSize * kGridSize);

		// Start from center
		int cx = kGridSize / 2, cy = kGridSize / 2;
		grid[cx][cy].active = true;
		int carved = 1;

		std::vector<std::pair<int,int>> frontier = {{cx,cy}};

		auto inBounds = [](int x, int y)
		{ return x >= 0 && x < kGridSize && y >= 0 && y < kGridSize; };

		static const int dx[] = {1,-1,0,0};
		static const int dy[] = {0,0,1,-1};

		while (carved < targetRooms && !frontier.empty())
		{
			int idx = std::rand() % (int)frontier.size();
			auto [fx, fy] = frontier[idx];

			// Try to expand into a random unvisited neighbour
			int dirs[4] = {0,1,2,3};
			for (int i = 3; i > 0; --i) std::swap(dirs[i], dirs[std::rand()%(i+1)]);

			bool expanded = false;
			for (int d : dirs)
			{
				int nx = fx + dx[d], ny = fy + dy[d];
				if (inBounds(nx,ny) && !grid[nx][ny].active)
				{
					grid[nx][ny].active = true;
					frontier.push_back({nx,ny});
					carved++;
					expanded = true;
					break;
				}
			}
			if (!expanded)
				frontier.erase(frontier.begin() + idx);
		}
	}

	// Assign corridors: a corridor connects adjacent active rooms
	bool AreConnected(const Grid& grid, int ax, int ay, int bx, int by)
	{
		return grid[ax][ay].active && grid[bx][by].active;
	}
}

namespace ProceduralMapSystem
{

void GenerateMap(std::shared_ptr<Pengine::Scene> scene, int raidDepth)
{
	std::srand(static_cast<unsigned>(raidDepth * 31337 + std::rand()));
	Pengine::Logger::Log("Generating procedural map, depth=" + std::to_string(raidDepth));

	auto cubeMesh     = Pengine::MeshManager::GetInstance().LoadMesh("Meshes/Cube.mesh");
	auto sphereMesh   = Pengine::MeshManager::GetInstance().LoadMesh("Meshes/Sphere.mesh");
	auto floorMat     = Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/WoodenFloor.mat");
	auto wallMat      = Pengine::MaterialManager::GetInstance().LoadMaterial("Materials/MeshBase.mat");
	auto extractMat   = Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/HealthPickUp.mat");
	auto dissolveMat  = Pengine::MaterialManager::GetInstance().LoadMaterial("Game/Assets/Materials/EnemyDissolve.mat");

	// ── Carve the room grid ──────────────────────────────────────────────────
	Grid grid = {};
	CarveRooms(grid, raidDepth);

	// Collect active rooms
	std::vector<std::pair<int,int>> activeRooms;
	for (int x = 0; x < kGridSize; ++x)
		for (int y = 0; y < kGridSize; ++y)
			if (grid[x][y].active)
				activeRooms.push_back({x,y});

	if (activeRooms.empty()) return;

	// Player spawn = center room (always active)
	int spawnGridX = kGridSize / 2, spawnGridY = kGridSize / 2;
	glm::vec3 playerSpawn = RoomWorldPos(spawnGridX, spawnGridY) + glm::vec3(0.0f, 2.0f, 0.0f);

	// Assign: farthest rooms = extraction points (2–3 of them)
	std::sort(activeRooms.begin(), activeRooms.end(), [&](auto& a, auto& b)
	{
		float da = std::pow((float)(a.first  - spawnGridX), 2) + std::pow((float)(a.second - spawnGridY), 2);
		float db = std::pow((float)(b.first  - spawnGridX), 2) + std::pow((float)(b.second - spawnGridY), 2);
		return da > db;
	});

	int numExtracts = 2 + (raidDepth >= 3 ? 1 : 0);
	for (int i = 0; i < std::min(numExtracts, (int)activeRooms.size()); ++i)
		grid[activeRooms[i].first][activeRooms[i].second].isExtract = true;

	// Assign enemies and loot to non-spawn, non-extract rooms
	int enemyIndex = 0;
	for (auto& [rx, ry] : activeRooms)
	{
		if (rx == spawnGridX && ry == spawnGridY) continue;
		if (grid[rx][ry].isExtract) continue;
		int roll = std::rand() % 100;
		if (roll < 60) grid[rx][ry].hasEnemy = true;
		if (roll < 80) grid[rx][ry].hasLoot  = true;
	}

	// ── Build geometry ───────────────────────────────────────────────────────
	float half = kRoomSize * 0.5f;
	float wallY = kWallH * 0.5f;

	auto inBounds = [](int x, int y)
	{ return x >= 0 && x < kGridSize && y >= 0 && y < kGridSize; };

	static const int dx4[] = {1,-1,0,0};
	static const int dy4[] = {0,0,1,-1};

	// Partial wall length on each side of a doorway opening
	const float kSideLen = half - kCorridorW * 0.5f;          // (18/2) - (4/2) = 7
	const float kSideOff = (half + kCorridorW * 0.5f) * 0.5f; // (9 + 2) / 2   = 5.5

	for (auto& [rx, ry] : activeRooms)
	{
		glm::vec3 roomCenter = RoomWorldPos(rx, ry);

		// Floor — rooms are directly adjacent so their floors tile seamlessly
		SpawnFloor(scene, cubeMesh, floorMat, roomCenter, kRoomSize, kRoomSize);

		for (int d = 0; d < 4; ++d)
		{
			int nx = rx + dx4[d], ny = ry + dy4[d];
			bool hasNeighbour = inBounds(nx,ny) && grid[nx][ny].active;

			if (!hasNeighbour)
			{
				// Solid exterior wall
				glm::vec3 wc = roomCenter;
				float wsx = kWallThick, wsz = kWallThick;
				if (d == 0) { wc.x += half; wsx = kWallThick; wsz = kRoomSize; }
				if (d == 1) { wc.x -= half; wsx = kWallThick; wsz = kRoomSize; }
				if (d == 2) { wc.z += half; wsx = kRoomSize;  wsz = kWallThick; }
				if (d == 3) { wc.z -= half; wsx = kRoomSize;  wsz = kWallThick; }
				wc.y = wallY;
				SpawnWall(scene, cubeMesh, wallMat, wc, wsx, kWallH, wsz);
			}
			else if (d == 0 || d == 2)
			{
				// Shared wall — spawn only from the "lower" room (d==0 or d==2) to avoid
				// duplicates. Two partial wall segments frame a kCorridorW-wide doorway.
				if (d == 0)
				{
					float faceX = roomCenter.x + half;
					SpawnWall(scene, cubeMesh, wallMat,
						{faceX, wallY, roomCenter.z - kSideOff}, kWallThick, kWallH, kSideLen);
					SpawnWall(scene, cubeMesh, wallMat,
						{faceX, wallY, roomCenter.z + kSideOff}, kWallThick, kWallH, kSideLen);
				}
				else // d == 2
				{
					float faceZ = roomCenter.z + half;
					SpawnWall(scene, cubeMesh, wallMat,
						{roomCenter.x - kSideOff, wallY, faceZ}, kSideLen, kWallH, kWallThick);
					SpawnWall(scene, cubeMesh, wallMat,
						{roomCenter.x + kSideOff, wallY, faceZ}, kSideLen, kWallH, kWallThick);
				}
			}
			// d==1 or d==3 with neighbour: the neighbour already placed the shared wall as d==0/d==2
		}

		// ── Room contents ────────────────────────────────────────────────
		if (grid[rx][ry].isExtract)
		{
			SpawnExtractionPoint(scene, cubeMesh, extractMat, roomCenter);
		}

		if (grid[rx][ry].hasLoot)
		{
			// 1–2 loot containers placed randomly in the room
			int n = 1 + std::rand() % 2;
			for (int i = 0; i < n; ++i)
			{
				float ox = ((float)(std::rand() % 100) / 100.0f - 0.5f) * (kRoomSize * 0.5f);
				float oz = ((float)(std::rand() % 100) / 100.0f - 0.5f) * (kRoomSize * 0.5f);
				LootSystem::SpawnLootContainer(scene, roomCenter + glm::vec3(ox, 0.0f, oz), raidDepth);
			}
		}

		if (grid[rx][ry].hasEnemy)
		{
			// 1–3 enemies per room
			int n = 1 + std::rand() % 3;

			// Generate patrol waypoints within the room
			std::vector<glm::vec3> waypoints;
			for (int i = 0; i < 4; ++i)
			{
				float ox = ((float)(std::rand() % 100) / 100.0f - 0.5f) * (kRoomSize * 0.6f);
				float oz = ((float)(std::rand() % 100) / 100.0f - 0.5f) * (kRoomSize * 0.6f);
				waypoints.push_back(roomCenter + glm::vec3(ox, 0.0f, oz));
			}

			for (int i = 0; i < n; ++i)
			{
				float ox = ((float)(std::rand() % 100) / 100.0f - 0.5f) * (kRoomSize * 0.35f);
				float oz = ((float)(std::rand() % 100) / 100.0f - 0.5f) * (kRoomSize * 0.35f);
				glm::vec3 spawnPos = roomCenter + glm::vec3(ox, 1.0f, oz);
				SpawnEnemy(scene, sphereMesh, dissolveMat, spawnPos, raidDepth, enemyIndex++, waypoints, RollEnemyType(raidDepth));
			}
		}
	}

	// ── Position player ──────────────────────────────────────────────────────
	auto playerEntity = scene->FindEntityByName("Player");
	if (playerEntity && playerEntity->HasComponent<Pengine::RigidBody>())
	{
		auto& rb = playerEntity->GetComponent<Pengine::RigidBody>();
		scene->GetPhysicsSystem()->Teleport(rb, playerSpawn, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
	}

	Pengine::Logger::Log("Map generated: " + std::to_string(activeRooms.size()) + " rooms, "
		+ std::to_string(enemyIndex) + " enemies");
}

} // namespace ProceduralMapSystem
