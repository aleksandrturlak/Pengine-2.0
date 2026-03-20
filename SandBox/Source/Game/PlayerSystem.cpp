#include "PlayerSystem.h"

#include "PlayerComponent.h"
#include "WeaponComponent.h"
#include "EnemyComponent.h"
#include "GameStateComponent.h"
#include "InventoryComponent.h"
#include "ShopComponent.h"
#include "LootContainerComponent.h"
#include "InteractableComponent.h"
#include "ItemComponent.h"

#include "Core/Scene.h"
#include "Core/Entity.h"
#include "Core/Input.h"
#include "Core/KeyCode.h"
#include "Core/Time.h"
#include "Core/WindowManager.h"
#include "Core/Window.h"
#include "Core/Raycast.h"
#include "Core/Logger.h"

#include "Components/Transform.h"
#include "Components/RigidBody.h"
#include "Components/Decal.h"

#include "Core/MaterialManager.h"

#include "ComponentSystems/PhysicsSystem.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

void PlayerSystem::OnUpdate(float dt, std::shared_ptr<Pengine::Scene> scene)
{
	auto window = Pengine::WindowManager::GetInstance().GetWindowByName("Main");
	if (!window)
		return;

	auto& input = Pengine::Input::GetInstance(window.get());

	// Check for game-over and home-base phase from GameStateComponent
	auto gsEntity = scene->FindEntityByName("GameState");
	bool gameOver = false;
	if (gsEntity && gsEntity->HasComponent<GameStateComponent>())
	{
		auto& gs = gsEntity->GetComponent<GameStateComponent>();
		gameOver = gs.GetPhase() == GameStateComponent::Phase::GameOver || gs.playerDied;
	}

	auto view = scene->GetRegistry().view<PlayerComponent>();
	for (auto handle : view)
	{
		PlayerComponent& player = view.get<PlayerComponent>(handle);

		// Handle game-over: unlock cursor once, then freeze input
		if (gameOver && !player.gameOverHandled)
		{
			window->ShowCursor();
			player.gameOverHandled = true;
		}
		if (gameOver)
			continue;

		if (!player.isAlive)
			continue;

		auto entity = scene->GetRegistry().get<Pengine::Transform>(handle).GetEntity();
		if (!entity)
			continue;

		// Check if inventory/shop/loot UI is open — freeze movement but allow inventory toggle
		bool uiOpen = false;
		if (entity->HasComponent<InventoryComponent>())
		{
			auto& inv = entity->GetComponent<InventoryComponent>();
			if (inv.inventoryOpen) uiOpen = true;

			// Check named interactable (shop, stash)
			auto near = scene->FindEntityByName(inv.nearInteractable);
			if (near)
			{
				if (near->HasComponent<ShopComponent>() && near->GetComponent<ShopComponent>().isOpen)
					uiOpen = true;
				if (near->HasComponent<InteractableComponent>() && near->GetComponent<InteractableComponent>().isOpen)
					uiOpen = true;
			}

			// Scan all loot containers — nearInteractable may already be cleared this frame
			if (!uiOpen)
			{
				auto lootView = scene->GetRegistry().view<LootContainerComponent>();
				for (auto lh : lootView)
				{
					if (lootView.get<LootContainerComponent>(lh).isOpen)
					{
						uiOpen = true;
						break;
					}
				}
			}
		}

		// Capture mouse on first update (unless UI is open)
		if (!player.cursorDisabled && !uiOpen)
		{
			window->DisableCursor();
			player.cursorDisabled = true;
		}

		if (uiOpen)
		{
			// Still tick timers but skip movement/look/shoot
			player.hitFlashTimer    = std::max(0.0f, player.hitFlashTimer    - dt);
			player.muzzleFlashTimer = std::max(0.0f, player.muzzleFlashTimer - dt);
			player.recoilTimer      = std::max(0.0f, player.recoilTimer      - dt);
			player.shootCooldown    = std::max(0.0f, player.shootCooldown    - dt);

			// Kill horizontal velocity so the player doesn't slide or fall off edges
			if (entity->HasComponent<Pengine::RigidBody>())
			{
				auto& rb = entity->GetComponent<Pengine::RigidBody>();
				if (rb.isValid)
				{
					auto physSys = scene->GetPhysicsSystem();
					auto& bodyInterface = physSys->GetInstance().GetBodyInterface();
					JPH::Vec3 vel = bodyInterface.GetLinearVelocity(rb.id);
					physSys->SetLinearVelocity(rb, glm::vec3(0.0f, vel.GetY(), 0.0f));
				}
			}
			continue;
		}

		// --- Mouse Look ---
		glm::dvec2 mouseDelta = input.GetMousePositionDelta();

		float yawDelta   = -(float)mouseDelta.x * player.mouseSensitivity * dt;
		float pitchDelta = -(float)mouseDelta.y * player.mouseSensitivity * dt;

		player.yawAngle   += yawDelta;
		player.pitchAngle += pitchDelta;

		constexpr float pitchLimit = glm::half_pi<float>() - 0.05f;
		player.pitchAngle = std::clamp(player.pitchAngle, -pitchLimit, pitchLimit);

		// Decay camera recoil toward 0
		float recoveryT = 1.0f - std::exp(-player.recoilRecovery * dt);
		player.recoilPitch = glm::mix(player.recoilPitch, 0.0f, recoveryT);
		player.recoilYaw   = glm::mix(player.recoilYaw,   0.0f, recoveryT);

		// Find camera child and apply pitch + recoil kick
		auto cameraEntity = entity->FindEntityInHierarchy("PlayerCamera");
		if (cameraEntity)
		{
			auto& camTransform = cameraEntity->GetComponent<Pengine::Transform>();
			float totalPitch = std::clamp(player.pitchAngle + player.recoilPitch, -(glm::half_pi<float>() - 0.05f), glm::half_pi<float>() - 0.05f);
			camTransform.Rotate(glm::vec3(totalPitch, 0.0f, 0.0f));
		}

		// --- Physics Movement ---
		if (!entity->HasComponent<Pengine::RigidBody>())
			continue;

		auto& rb = entity->GetComponent<Pengine::RigidBody>();
		if (!rb.isValid)
			continue;

		auto physSys = scene->GetPhysicsSystem();
		auto& bodyInterface = physSys->GetInstance().GetBodyInterface();

		JPH::Vec3 currentVel = bodyInterface.GetLinearVelocity(rb.id);
		float currentYVel = currentVel.GetY();

		glm::quat yawQuat = glm::angleAxis(player.yawAngle + player.recoilYaw, glm::vec3(0.0f, 1.0f, 0.0f));
		bodyInterface.SetRotation(rb.id, Pengine::GlmQuatToJoltQuat(yawQuat), JPH::EActivation::Activate);

		physSys->SetAngularVelocity(rb, glm::vec3(0.0f));

		float yaw = player.yawAngle;
		glm::vec3 forward(-std::sin(yaw), 0.0f, -std::cos(yaw));
		glm::vec3 right(std::cos(yaw), 0.0f, -std::sin(yaw));

		glm::vec3 moveDir(0.0f);
		if (input.IsKeyDown(Pengine::KeyCode::KEY_W)) moveDir += forward;
		if (input.IsKeyDown(Pengine::KeyCode::KEY_S)) moveDir -= forward;
		if (input.IsKeyDown(Pengine::KeyCode::KEY_D)) moveDir += right;
		if (input.IsKeyDown(Pengine::KeyCode::KEY_A)) moveDir -= right;

		glm::vec3 targetVel(0.0f);
		if (glm::length(moveDir) > 0.001f)
			targetVel = glm::normalize(moveDir) * player.moveSpeed;

		targetVel.y = currentYVel;
		physSys->SetLinearVelocity(rb, targetVel);

		// --- Jump ---
		Pengine::Raycast::PhysicsHit groundHit;
		glm::vec3 playerPos = entity->GetComponent<Pengine::Transform>().GetPosition();
		bool isGrounded = Pengine::Raycast::RaycastPhysics(
			scene, playerPos, glm::vec3(0.0f, -1.0f, 0.0f), 1.15f, groundHit, entity);
		if (input.IsKeyPressed(Pengine::KeyCode::SPACE) && isGrounded)
		{
			glm::vec3 jumpVel = targetVel;
			jumpVel.y = player.jumpSpeed;
			physSys->SetLinearVelocity(rb, jumpVel);
		}

		// --- Hit flash ---
		player.hitFlashTimer = std::max(0.0f, player.hitFlashTimer - dt);

		// --- Muzzle flash ---
		player.muzzleFlashTimer = std::max(0.0f, player.muzzleFlashTimer - dt);

		// --- Visual recoil ---
		player.recoilTimer = std::max(0.0f, player.recoilTimer - dt);
		if (cameraEntity)
		{
			for (auto& weakChild : cameraEntity->GetChilds())
			{
				auto child = weakChild.lock();
				if (!child || !child->HasComponent<WeaponComponent>()) continue;

				auto& wc = child->GetComponent<WeaponComponent>();
				if (wc.slot != player.activeWeaponSlot) continue;

				float progress = 1.0f - (player.recoilTimer / player.recoilDuration);
				float kick     = std::sin(progress * glm::pi<float>());

				bool  isMoving  = glm::length(glm::vec2(targetVel.x, targetVel.z)) > 0.1f;
				float swaySpeed = isMoving ? 3.8f  : 2.2f;
				float swayAmpX  = isMoving ? 0.010f : 0.003f;
				float swayAmpY  = isMoving ? 0.005f : 0.0015f;

				if (!wc.isReloading)
					wc.swayTime += dt * swaySpeed;
				float swayMult = wc.isReloading ? 0.0f : 1.0f;
				float swayX = std::sin(wc.swayTime)        * swayAmpX * swayMult;
				float swayY = std::sin(wc.swayTime * 2.0f) * swayAmpY * swayMult;

				glm::vec3 reloadOffset(0.0f);
				glm::vec3 reloadRotation(0.0f);
				if (wc.isReloading)
				{
					float reloadProgress = 1.0f - (wc.reloadTimer / wc.reloadTime);
					float dropT = std::sin(reloadProgress * glm::pi<float>());
					reloadOffset.y    = -dropT * 0.09f;
					reloadOffset.z    =  dropT * 0.03f;
					reloadRotation.z  =  dropT * glm::radians(30.0f);
					reloadRotation.x  = -dropT * glm::radians(15.0f);
				}

				glm::vec3 offset(swayX + reloadOffset.x,
				                 swayY + kick * wc.kickUp  + reloadOffset.y,
				                 kick * wc.kickBack        + reloadOffset.z);
				auto& wTransform = child->GetComponent<Pengine::Transform>();
				wTransform.Translate(wc.restPosition + offset);
				wTransform.Rotate(reloadRotation);

				if (auto muzzleFlash = child->FindEntityInHierarchy("MuzzleFlash"))
					muzzleFlash->SetEnabled(player.muzzleFlashTimer > 0.0f);
				break;
			}
		}

		// --- Shoot / Reload ---
		player.shootCooldown = std::max(0.0f, player.shootCooldown - dt);

		InventoryComponent* invPtr = entity->HasComponent<InventoryComponent>()
			? &entity->GetComponent<InventoryComponent>() : nullptr;

		if (cameraEntity)
		{
			WeaponComponent* wc = nullptr;
			for (auto& weakChild : cameraEntity->GetChilds())
			{
				auto child = weakChild.lock();
				if (!child || !child->HasComponent<WeaponComponent>() || !child->IsEnabled()) continue;
				auto& w = child->GetComponent<WeaponComponent>();
				if (w.slot == player.activeWeaponSlot) { wc = &w; break; }
			}

			if (wc)
			{
				// Count total reserve ammo from grid ammo stacks matching this weapon's type
				auto countReserve = [&]() -> int {
					if (!invPtr) return 0;
					int total = 0;
					for (int i = 0; i < InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols; ++i)
					{
						const auto& s = invPtr->grid[i];
						if (s.occupied && s.itemTypeInt == static_cast<int>(ItemComponent::Type::Ammo)
							&& s.ammoTypeInt == wc->ammoTypeInt)
							total += s.ammoCount;
					}
					return total;
				};
				// Consume ammo from grid stacks (back to front, partial stacks first)
				auto consumeAmmo = [&](int amount) {
					if (!invPtr || amount <= 0) return;
					for (int i = InventoryComponent::kMaxGridRows * InventoryComponent::kMaxGridCols - 1;
						i >= 0 && amount > 0; --i)
					{
						auto& s = invPtr->grid[i];
						if (!s.occupied || s.itemTypeInt != static_cast<int>(ItemComponent::Type::Ammo)
							|| s.ammoTypeInt != wc->ammoTypeInt) continue;
						int take = std::min(amount, s.ammoCount);
						s.ammoCount -= take;
						amount      -= take;
						if (s.ammoCount <= 0) s = {};
					}
				};

				if (wc->isReloading)
				{
					wc->reloadTimer -= dt;
					if (wc->reloadTimer <= 0.0f)
					{
						wc->isReloading = false;
						int take = std::min(wc->magazineSize - wc->currentAmmo, countReserve());
						wc->currentAmmo += take;
						consumeAmmo(take);
					}
				}

				if (input.IsKeyPressed(Pengine::KeyCode::KEY_R)
					&& !wc->isReloading
					&& wc->currentAmmo < wc->magazineSize
					&& countReserve() > 0)
				{
					wc->isReloading = true;
					wc->reloadTimer = wc->reloadTime;
				}

				bool triggerHeld = input.IsMouseDown(Pengine::KeyCode::MOUSE_BUTTON_1);
				if (triggerHeld && player.shootCooldown <= 0.0f && !wc->isReloading)
				{
					if (wc->currentAmmo > 0)
					{
						wc->currentAmmo--;
						player.shootCooldown  = wc->fireRate;
						player.recoilTimer    = player.recoilDuration;
						player.recoilPitch   += wc->cameraRecoilUp;
						float side = ((float)(std::rand() % 201) / 100.0f - 1.0f) * wc->cameraRecoilSide;
						player.recoilYaw     += side;
						player.recoilRecovery = wc->recoilRecovery;
						player.muzzleFlashTimer = player.muzzleFlashDuration;

						auto& camTransform = cameraEntity->GetComponent<Pengine::Transform>();
						glm::vec3 camPos   = camTransform.GetPosition();
						glm::vec3 camFwd   = camTransform.GetForward();
						glm::vec3 camRight = camTransform.GetRight();
						glm::vec3 camUp    = camTransform.GetUp();
						glm::vec3 origin   = camPos + camFwd * 0.3f;

						if (wc->physicalRecoilForce > 0.0f)
						{
							glm::vec3 kick = -camFwd * wc->physicalRecoilForce;
							bodyInterface.AddLinearVelocity(rb.id, JPH::Vec3(kick.x, kick.y, kick.z));
						}

						for (int pellet = 0; pellet < wc->pelletCount; ++pellet)
						{
							glm::vec3 shotDir = camFwd;
							if (wc->spreadAngle > 0.0f)
							{
								float rx = ((float)(std::rand() % 2001) / 1000.0f - 1.0f) * wc->spreadAngle;
								float ry = ((float)(std::rand() % 2001) / 1000.0f - 1.0f) * wc->spreadAngle;
								shotDir = glm::normalize(camFwd + camRight * rx + camUp * ry);
							}

							Pengine::Raycast::PhysicsHit hit;
							if (Pengine::Raycast::RaycastPhysics(scene, origin, shotDir, 150.0f, hit, entity))
							{
								if (hit.entity)
								{
									if (hit.entity->HasComponent<EnemyComponent>())
										hit.entity->GetComponent<EnemyComponent>().health -= wc->damage;

									auto decalEntity = scene->CreateEntity("BulletHoleDecal");
									auto& decalComp  = decalEntity->AddComponent<Pengine::Decal>();
									decalComp.material = Pengine::MaterialManager::GetInstance().LoadMaterial(
										"Game/Assets/Materials/BulletHole.mat");
									auto& t = decalEntity->AddComponent<Pengine::Transform>(decalEntity);
									t.Scale({ 0.1f, 0.1f, 0.1f });

									glm::vec3 normal = glm::normalize(hit.normal);
									glm::vec3 worldUp = glm::abs(glm::dot(normal, glm::vec3(0.f, 1.f, 0.f))) < 0.999f
										? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f);
									glm::vec3 decalRight = glm::normalize(glm::cross(worldUp, normal));
									glm::vec3 decalUp    = glm::cross(normal, decalRight);

									glm::quat baseRot = glm::quat_cast(glm::mat3(decalRight, decalUp, normal));
									glm::vec3 decalPos = hit.point + normal * 0.02f;
									t.Translate(decalPos);
									t.Rotate(glm::eulerAngles(baseRot) + glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f));

									hit.entity->AddChild(decalEntity);
								}
							}
						}
					}
					else if (countReserve() > 0)
					{
						wc->isReloading = true;
						wc->reloadTimer = wc->reloadTime;
					}
				}
			}
		}
	}
}
