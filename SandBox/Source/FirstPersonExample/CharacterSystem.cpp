#include "CharacterSystem.h"

#include "Core/Scene.h"
#include "Core/MeshManager.h"
#include "Core/MaterialManager.h"
#include "Core/Input.h"
#include "Core/KeyCode.h"
#include "Core/Viewport.h"
#include "Core/ClayManager.h"
#include "Core/WindowManager.h"
#include "Core/TextureManager.h"
#include "Core/Logger.h"
#include "Core/RandomGenerator.h"

#include "Components/Transform.h"
#include "Components/SkeletalAnimator.h"
#include "Components/Decal.h"
#include "Components/RigidBody.h"

#include "ComponentSystems/PhysicsSystem.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

#include "../Components/Character.h"
#include "../Components/Enemy.h"
#include "../Components/UI.h"

using namespace Pengine;

void addState(FirstPersonCharacter& character, State state, std::unique_ptr<CharacterState> characterState)
{
	character.states[state] = std::move(characterState);
}

void setState(entt::registry& registry, entt::entity entity, State state)
{
	FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);
	auto foundState = character.states.find(state);
	if (foundState != character.states.end() && foundState->second.get() != character.currentState)
	{
		if (character.currentState)
		{
			character.currentState->Exit(registry, entity);
		}

		character.currentState = foundState->second.get();
		character.currentState->Enter(registry, entity);
	}
}

class IdleState : public CharacterState
{
public:
	void Enter(entt::registry& registry, entt::entity entity) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);
		SkeletalAnimator& skeletalAnimator = registry.get<SkeletalAnimator>(entity);

		skeletalAnimator.CrossfadeLayer(0, character.animations[State::IDLE], 0.1f);
	}

	void Update(entt::registry& registry, entt::entity entity, float deltaTime) override
	{
	}

	void Exit(entt::registry& registry, entt::entity entity) override
	{
	}

	void HandleInput(entt::registry& registry, entt::entity entity, Pengine::Input& input) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);
		
		if (input.IsKeyPressed(KeyCode::KEY_R) && character.currentMagazine < character.maxMagazine && character.currentAmmo > 0)
		{
			setState(registry, entity, State::RELOAD);
		}
		else if (character.isShotPressed && character.currentMagazine > 0)
		{
			setState(registry, entity, State::SHOT);
		}
		else if (character.isMovePressed)
		{
			setState(registry, entity, State::WALK);
		}
	}

	State GetState() const override { return State::IDLE; }
};

class WalkingState : public CharacterState {
private:
	float currentSpeed = 0.0f;
	bool isWalking = true;

public:
	void Enter(entt::registry& registry, entt::entity entity) override
	{
	}

	void Update(entt::registry& registry, entt::entity entity, float deltaTime) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);
		SkeletalAnimator& skeletalAnimator = registry.get<SkeletalAnimator>(entity);

		const auto linearSpeed = character.joltCharacter->GetLinearVelocity();
		const float speed = JPH::Vec3(linearSpeed.GetX(), 0.0f, linearSpeed.GetZ()).Length();
		if (speed > 0.0f && speed < character.speed)
		{
			const float factor = speed / character.speed;
			skeletalAnimator.SetLayerAnimation(0, character.animations[State::IDLE]);
			skeletalAnimator.SetLayerWeight(0, 1.0f);
			skeletalAnimator.SetLayerAnimation(1, character.animations[State::WALK]);
			skeletalAnimator.SetLayerWeight(1, factor);

			isWalking = true;
		}
		else if (speed >= character.speed)
		{
			const float factor = glm::abs(speed - character.speed) / character.speed;
			skeletalAnimator.SetLayerAnimation(0, character.animations[State::WALK]);
			skeletalAnimator.SetLayerWeight(0, 1.0f);
			skeletalAnimator.SetLayerAnimation(1, character.animations[State::RUN]);
			skeletalAnimator.SetLayerWeight(1, factor);

			isWalking = false;
		}
	}

	void Exit(entt::registry& registry, entt::entity entity) override
	{
		SkeletalAnimator& skeletalAnimator = registry.get<SkeletalAnimator>(entity);
		if (skeletalAnimator.GetLayerCount() > 1 && skeletalAnimator.GetLayer(1).animation)
		{
			skeletalAnimator.SetLayerAnimation(0, skeletalAnimator.GetLayer(1).animation);
		}
		if (skeletalAnimator.GetLayerCount() > 1)
		{
			skeletalAnimator.SetLayerWeight(1, 0.0f);
		}
	}

	void HandleInput(entt::registry& registry, entt::entity entity, Pengine::Input& input) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);

		if (isWalking && input.IsKeyPressed(KeyCode::KEY_R) && character.currentMagazine < character.maxMagazine && character.currentAmmo > 0)
		{
			setState(registry, entity, State::RELOAD);
		}
		else if (isWalking && character.isShotPressed && character.currentMagazine > 0)
		{
			setState(registry, entity, State::SHOT);
		}
		else if (!character.isMovePressed)
		{
			setState(registry, entity, State::IDLE);
		}
	}

	State GetState() const override { return State::WALK; }
};

class ReloadState : public CharacterState
{
public:
	float timer = 0.0f;

	void Enter(entt::registry& registry, entt::entity entity) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);
		SkeletalAnimator& skeletalAnimator = registry.get<SkeletalAnimator>(entity);

		skeletalAnimator.CrossfadeLayer(0, character.animations[State::RELOAD], 0.2f);
		
		timer = 0.0f;
	}

	void Update(entt::registry& registry, entt::entity entity, float deltaTime) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);

		timer += deltaTime;
		if (timer > character.animations[State::RELOAD]->GetDuration())
		{
			timer = 0.0f;
			if (character.isShotPressed && character.currentMagazine > 0)
			{
				setState(registry, entity, State::SHOT);
			}
			else if (character.isMovePressed)
			{
				setState(registry, entity, State::WALK);
			}
			else
			{
				setState(registry, entity, State::IDLE);
			}
		}
	}

	void Exit(entt::registry& registry, entt::entity entity) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);
		SkeletalAnimator& skeletalAnimator = registry.get<SkeletalAnimator>(entity);

		skeletalAnimator.SetLayerAnimation(0, character.animations[State::IDLE]);

		if (character.currentAmmo > 0 || character.currentMagazine < character.maxMagazine)
		{
			const int ammoNeeded = character.maxMagazine - character.currentMagazine;
			const int ammoToTake = ammoNeeded <= character.currentAmmo ? ammoNeeded : character.currentAmmo;

			character.currentMagazine += ammoToTake;
			character.currentAmmo -= ammoToTake;
		}
	}

	void HandleInput(entt::registry& registry, entt::entity entity, Pengine::Input& input) override
	{
	}

	State GetState() const override { return State::RELOAD; }
};

class ShotState : public CharacterState
{
public:
	float timer = 0.0f;

	void Enter(entt::registry& registry, entt::entity entity) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);
		SkeletalAnimator& skeletalAnimator = registry.get<SkeletalAnimator>(entity);

		skeletalAnimator.CrossfadeLayer(0, character.animations[State::SHOT], 0.0f);

		timer = 0.0f;
	}

	void Update(entt::registry& registry, entt::entity entity, float deltaTime) override
	{
		timer += deltaTime;
	}

	void Exit(entt::registry& registry, entt::entity entity) override
	{
	}

	void HandleInput(entt::registry& registry, entt::entity entity, Pengine::Input& input) override
	{
		FirstPersonCharacter& character = registry.get<FirstPersonCharacter>(entity);
		SkeletalAnimator& skeletalAnimator = registry.get<SkeletalAnimator>(entity);

		if (timer > character.animations[State::SHOT]->GetDuration())
		{
			timer = 0.0f;
			if (input.IsKeyPressed(KeyCode::KEY_R) && character.currentMagazine < character.maxMagazine && character.currentAmmo > 0)
			{
				setState(registry, entity, State::RELOAD);
			}
			else if (character.isMovePressed)
			{
				setState(registry, entity, State::WALK);
			}
			else
			{
				skeletalAnimator.SetLayerAnimation(0, character.animations[State::IDLE]);
				setState(registry, entity, State::IDLE);
			}
		}
	}

	State GetState() const override { return State::SHOT; }
};

void HandleInputFirstPerson(JPH::Character* mCharacter, float sCharacterSpeed, float sCharacterJumpSpeed, JPH::Vec3Arg inMovementDirection, bool inJump, float inDeltaTime)
{
	bool sControlMovementDuringJump = true;

	using namespace JPH;
	// Cancel movement in opposite direction of normal when touching something we can't walk up
	Vec3 movement_direction = inMovementDirection;
	Character::EGroundState ground_state = mCharacter->GetGroundState();
	if (ground_state == Character::EGroundState::OnSteepGround
		|| ground_state == Character::EGroundState::NotSupported)
	{
		Vec3 normal = mCharacter->GetGroundNormal();
		normal.SetY(0.0f);
		float dot = normal.Dot(movement_direction);
		if (dot < 0.0f)
			movement_direction -= (dot * normal) / normal.LengthSq();
	}

	if (sControlMovementDuringJump || mCharacter->IsSupported())
	{
		// Update velocity
		Vec3 current_velocity = mCharacter->GetLinearVelocity();
		Vec3 desired_velocity = sCharacterSpeed * movement_direction;
		if (!desired_velocity.IsNearZero() || current_velocity.GetY() < 0.0f || !mCharacter->IsSupported())
			desired_velocity.SetY(current_velocity.GetY());
		Vec3 new_velocity = 0.75f * current_velocity + 0.25f * desired_velocity;

		// Jump
		if (inJump && ground_state == Character::EGroundState::OnGround)
			new_velocity.SetY(sCharacterJumpSpeed);

		// Update the velocity
		mCharacter->SetLinearVelocity(new_velocity);
	}
}

CharacterSystem::CharacterSystem()
{
	JPH::RegisterDefaultAllocator();

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	auto callback = [this](std::shared_ptr<Entity> entity)
	{
		if (!entity || !entity->HasComponent<FirstPersonCharacter>())
		{
			return;
		}

		FirstPersonCharacter& character = entity->GetComponent<FirstPersonCharacter>();
		character.joltCharacter->RemoveFromPhysicsSystem();
	};
	m_RemoveCallbacks[GetTypeName<FirstPersonCharacter>()] = callback;

	m_BloodDecalMaterails.emplace_back(MaterialManager::GetInstance().LoadMaterial("BloodDecals/BloodDecal1.mat"));
	m_BloodDecalMaterails.emplace_back(MaterialManager::GetInstance().LoadMaterial("BloodDecals/BloodDecal2.mat"));
	m_BloodDecalMaterails.emplace_back(MaterialManager::GetInstance().LoadMaterial("BloodDecals/BloodDecal3.mat"));
}

CharacterSystem::~CharacterSystem()
{
	WindowManager::GetInstance().GetCurrentWindow()->ShowCursor();
}

void CharacterSystem::OnUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
{
	//WindowManager::GetInstance().GetCurrentWindow()->DisableCursor();

	m_WeakScene = scene;

	auto& input = Input::GetInstance(WindowManager::GetInstance().GetWindowByName("Main").get());

	auto& joltPhysicsSystem = scene->GetPhysicsSystem()->GetInstance();

	const auto& view = scene->GetRegistry().view<FirstPersonCharacter>();
	for (const entt::entity entity : view)
	{
		FirstPersonCharacter& character = scene->GetRegistry().get<FirstPersonCharacter>(entity);
		Transform& transform = scene->GetRegistry().get<Transform>(entity);
		SkeletalAnimator& skeletalAnimator = scene->GetRegistry().get<SkeletalAnimator>(entity);

		if (character.animations.empty())
		{
			std::filesystem::path animFilepath = "Examples/FirstPerson/Assets";
			character.animations[State::IDLE] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Idle.anim");
			character.animations[State::WALK] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Walk.anim");
			character.animations[State::RUN] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Run.anim");
			character.animations[State::SHOT] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Shoot.anim");
			character.animations[State::INSPECT] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Inspect.anim");
			character.animations[State::RELOAD] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Reload.anim");

			addState(character, State::IDLE, std::make_unique<IdleState>());
			addState(character, State::WALK, std::make_unique<WalkingState>());
			addState(character, State::RELOAD, std::make_unique<ReloadState>());
			addState(character, State::SHOT, std::make_unique<ShotState>());

			// Layer 0: base animation. Layer 1: blend target (walk/run locomotion blend).
			SkeletalAnimator::AnimationLayer baseLayer;
			baseLayer.animation = character.animations[State::IDLE];
			baseLayer.speed = 1.0f;
			baseLayer.weight = 1.0f;
			skeletalAnimator.AddLayer(baseLayer);

			SkeletalAnimator::AnimationLayer blendLayer;
			blendLayer.weight = 0.0f;
			skeletalAnimator.AddLayer(blendLayer);

			setState(scene->GetRegistry(), entity, State::IDLE);
		}

		if (!character.joltCharacter)
		{
			JPH::CharacterSettings settings;
			settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
			settings.mLayer = ObjectLayers::DYNAMIC;
			settings.mShape = JPH::RotatedTranslatedShapeSettings(
				JPH::Vec3(0, -0.5f * character.height, 0),
				JPH::Quat::sIdentity(),
				new JPH::CapsuleShape(0.5f * character.height, character.radius)).Create().Get();

			settings.mFriction = 0.5f;
			settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -character.radius);

			character.joltCharacter = std::make_unique<JPH::Character>(
				&settings,
				GlmVec3ToJoltVec3(transform.GetPosition()),
				GlmQuatToJoltQuat(transform.GetRotation()),
				0,
				&joltPhysicsSystem);
			character.joltCharacter->AddToPhysicsSystem();
		}

		const glm::vec3 bottomCenter = transform.GetPosition() - glm::vec3(0.0f, character.height, 0.0f);
		const glm::vec3 topCenter = transform.GetPosition() + glm::vec3(0.0f, character.height, 0.0f);
		//scene->GetVisualizer().DrawCapsule(bottomCenter, topCenter, { 1.0f, 1.0f, 0.0f }, 0.3);

		character.isMovePressed = input.IsKeyDown(KeyCode::KEY_W)
			|| input.IsKeyDown(KeyCode::KEY_S)
			|| input.IsKeyDown(KeyCode::KEY_D)
			|| input.IsKeyDown(KeyCode::KEY_A);

		character.isJumpPressed = input.IsKeyDown(KeyCode::SPACE);

		character.isShotPressed = input.IsMouseDown(KeyCode::MOUSE_BUTTON_1);

		if (character.currentState)
		{
			character.currentState->HandleInput(scene->GetRegistry(), entity, input);
			character.currentState->Update(scene->GetRegistry(), entity, deltaTime);
		}

		glm::vec3 direction{};

		if (input.IsKeyDown(KeyCode::KEY_A))
		{
			direction += -transform.GetRight();
		}
		if (input.IsKeyDown(KeyCode::KEY_D))
		{
			direction += transform.GetRight();
		}
		if (input.IsKeyDown(KeyCode::KEY_W))
		{
			direction += transform.GetForward();
		}
		if (input.IsKeyDown(KeyCode::KEY_S))
		{
			direction += transform.GetBack();
		}

		const bool canRun = character.currentState->GetState() != State::RELOAD && character.currentState->GetState() != State::SHOT;
		const bool runPressed = input.IsKeyDown(KeyCode::KEY_LEFT_SHIFT);

		float characterSpeed = (runPressed && canRun) ? character.speed * 2.0f : character.speed;

		HandleInputFirstPerson(character.joltCharacter.get(), characterSpeed, character.jump, GlmVec3ToJoltVec3(direction).NormalizedOr({}), character.isJumpPressed, deltaTime);
	}
}

std::shared_ptr<Entity> CharacterSystem::CreateDecal(std::shared_ptr<Scene> scene, const glm::vec3& position, const glm::vec3& normal)
{
	std::shared_ptr<Entity> entity = scene->CreateEntity("Decal");

	Transform& transform = entity->AddComponent<Transform>(entity);
	transform.Translate(position);
	transform.Scale(glm::vec3(0.2f));

	const float yaw = atan2(normal.x, normal.z);
	const float pitch = asin(normal.y);

	transform.Rotate(glm::vec3(glm::radians(90.0f), 0.0f, 0.0f));
	transform.Rotate(transform.GetRotation() + glm::vec3(pitch, yaw, 0.0f));
	Decal& decal = entity->AddComponent<Decal>();
	decal.material = m_BloodDecalMaterails[RandomGenerator::GetInstance().Get(0, 2)];

	return entity;
}

void CharacterSystem::OnPrePhysicsUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
{
	auto& input = Input::GetInstance(WindowManager::GetInstance().GetWindowByName("Main").get());
	auto& joltPhysicsSystem = scene->GetPhysicsSystem()->GetInstance();

	const auto& view = scene->GetRegistry().view<FirstPersonCharacter>();
	for (const entt::entity entity : view)
	{
		FirstPersonCharacter& character = scene->GetRegistry().get<FirstPersonCharacter>(entity);
		Transform& transform = scene->GetRegistry().get<Transform>(entity);

		glm::vec2 delta = glm::vec2(input.GetMousePositionDelta());

		auto rotation = GlmQuatToJoltQuat(transform.GetRotation());
		const JPH::Quat yRot = JPH::Quat::sRotation(JPH::Vec3(0, 1, 0), -delta.x * deltaTime * JPH::DegreesToRadians(90.0f));

		const auto camera = transform.GetEntity()->FindEntityInHierarchy("Camera");
		Transform& cameraTransform = camera->GetComponent<Transform>();
		const float pitch = -delta.y * deltaTime * JPH::DegreesToRadians(90.0f);
		glm::vec3 cameraRotation = cameraTransform.GetRotation(Transform::System::LOCAL) + glm::vec3(pitch, 0.0f, 0.0f);
		cameraRotation.x = glm::clamp(cameraRotation.x, -glm::half_pi<float>(), glm::half_pi<float>());
		cameraTransform.Rotate(cameraRotation);

		character.joltCharacter->SetPositionAndRotation(
			GlmVec3ToJoltVec3(transform.GetPosition()),
			yRot * rotation);

		if (character.currentState->GetState() != State::SHOT)
		{
			return;
		}

		const bool cantShot = character.shotTimer > 0.0f;
		character.shotTimer += deltaTime;
		if (character.shotTimer >= character.animations[State::SHOT]->GetDuration())
		{
			character.shotTimer = 0.0f;
		}

		if (cantShot)
		{
			return;
		}

		character.currentMagazine--;

		class IgnoreSingleBodyCollector : public JPH::CastRayCollector
		{
		public:
			IgnoreSingleBodyCollector(JPH::BodyID bodyToIgnore) : m_BodyToIgnore(bodyToIgnore) {}

			virtual void AddHit(const JPH::RayCastResult& inResult) override
			{
				if (inResult.mBodyID == m_BodyToIgnore)
					return;

				hits.emplace_back(inResult);
			}

			std::vector<JPH::RayCastResult> hits;
		private:
			JPH::BodyID m_BodyToIgnore;
		};

		const JPH::Vec3 origin = GlmVec3ToJoltVec3(cameraTransform.GetPosition());
		const JPH::Vec3 direction = GlmVec3ToJoltVec3(cameraTransform.GetForward() * 100.0f);

		JPH::RRayCast ray(origin, direction);
		JPH::RayCastSettings settings{};

		IgnoreSingleBodyCollector collector(character.joltCharacter->GetBodyID());
		joltPhysicsSystem.GetNarrowPhaseQuery().CastRay(
			ray, settings, collector);
		
		std::sort(collector.hits.begin(), collector.hits.end(), [](const JPH::RayCastResult& a, const JPH::RayCastResult& b)
		{
			return a.mFraction < b.mFraction;
		});

		if (!collector.hits.empty())
		{
			const auto result = *collector.hits.begin();
			JPH::Vec3 point = origin + direction * result.mFraction;
			joltPhysicsSystem.GetBodyInterface().AddForce(result.mBodyID, GlmVec3ToJoltVec3(cameraTransform.GetForward() * character.bulletForce), point, JPH::EActivation::Activate);
			
			scene->GetVisualizer().DrawLine(JoltVec3ToGlmVec3(origin), JoltVec3ToGlmVec3(point), { 1.0f, 0.0f, 0.0f }, 1.0f);
			scene->GetVisualizer().DrawSphere({ 1.0f, 0.0f, 0.0f }, glm::translate(glm::mat4(1.0f), JoltVec3ToGlmVec3(point)), 0.3f, 10, 1.0f);

			const entt::entity handle = scene->GetPhysicsSystem()->GetEntity(result.mBodyID);
			auto entity = scene->GetRegistry().get<Transform>(handle).GetEntity();

			JPH::BodyLockRead lock(joltPhysicsSystem.GetBodyLockInterface(), result.mBodyID);
			if (lock.Succeeded())
			{
				const JPH::Body& body = lock.GetBody();
				JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point);
				const auto decal = CreateDecal(scene, JoltVec3ToGlmVec3(point), JoltVec3ToGlmVec3(normal));

				if (RigidBody* rigidbody = scene->GetRegistry().try_get<RigidBody>(handle))
				{
					if (!rigidbody->isStatic)
					{
						entity->AddChild(decal);
					}
				}

				scene->GetVisualizer().DrawLine(JoltVec3ToGlmVec3(point), JoltVec3ToGlmVec3(point + normal), { 1.0f, 1.0f, 0.0f }, 1.0f);
			}

			if (Enemy* enemy = scene->GetRegistry().try_get<Enemy>(handle))
			{
				enemy->health -= character.damage;
				if (enemy->health <= 0.0f)
				{	
					scene->DeleteEntity(entity);
				}
			}
		}
	}
}

void CharacterSystem::OnPostPhysicsUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
{
	const auto& view = scene->GetRegistry().view<FirstPersonCharacter>();
	for (const entt::entity entity : view)
	{
		FirstPersonCharacter& character = scene->GetRegistry().get<FirstPersonCharacter>(entity);
		Transform& transform = scene->GetRegistry().get<Transform>(entity);

		character.joltCharacter->PostSimulation(0.1f);

		transform.Translate(JoltVec3ToGlmVec3(character.joltCharacter->GetPosition()));
		transform.Rotate(glm::eulerAngles(JoltQuatToGlmQuat(character.joltCharacter->GetRotation())));
	}
}
