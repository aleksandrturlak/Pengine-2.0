#include "FirstPersonCharacterSystem.h"

#include "Core/Scene.h"
#include "Core/MeshManager.h"
#include "Core/Input.h"
#include "Core/KeyCode.h"
#include "Core/Viewport.h"
#include "Core/ClayManager.h"
#include "Core/WindowManager.h"
#include "Core/TextureManager.h"

#include "Components/Transform.h"
#include "Components/SkeletalAnimator.h"

#include "ComponentSystems/PhysicsSystem.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

#define CLAY_IMPLEMENTATION
#include "Components/Canvas.h"

using namespace Pengine;

void HandleInputFirstPerson(JPH::Character* mCharacter, float sCharacterSpeed, JPH::Vec3Arg inMovementDirection, bool inJump, float inDeltaTime)
{
	bool sControlMovementDuringJump = true;
	float sJumpSpeed = 10.0f;

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
			new_velocity += Vec3(0, sJumpSpeed, 0);

		// Update the velocity
		mCharacter->SetLinearVelocity(new_velocity);
	}
}

FirstPersonCharacterSystem::FirstPersonCharacterSystem()
{
	JPH::RegisterDefaultAllocator();

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	ClayManager::GetInstance().scriptsByName["CrossHair"] = [this](Canvas* canvas, std::shared_ptr<Entity> entity)
	{
		ClayManager::BeginLayout();

		ClayManager::OpenElement();
		ClayManager::ConfigureOpenElement(
		{
			.layout =
				{
					.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() },
					.childAlignment =
						{
							.x = Clay_LayoutAlignmentX::CLAY_ALIGN_X_CENTER,
							.y = Clay_LayoutAlignmentY::CLAY_ALIGN_Y_CENTER,
						},
				}
		});
		{
			ClayManager::OpenElement();
			ClayManager::ConfigureOpenElement(
			{
				.id = CLAY_ID("CrossHair"),
				.layout =
					{
						.sizing = {.width = CLAY_SIZING_FIXED(64), .height = CLAY_SIZING_FIXED(64) },
						.childAlignment =
						{
							.x = Clay_LayoutAlignmentX::CLAY_ALIGN_X_CENTER,
							.y = Clay_LayoutAlignmentY::CLAY_ALIGN_Y_CENTER,
						},
						.layoutDirection = Clay_LayoutDirection::CLAY_TOP_TO_BOTTOM,
					},
				.backgroundColor = { 1.0f, 0.0f, 0.0f, 1.0f },
				.image =
				{
					.imageData = TextureManager::GetInstance().Load("Scenes/Examples/FirstPerson/Assets/textures/CrossHair.png").get(),
				},
			});
			ClayManager::CloseElement();
		}

		ClayManager::CloseElement();

		return ClayManager::EndLayout();
	};
}

void FirstPersonCharacterSystem::OnUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
{
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
			std::filesystem::path animFilepath = "Scenes/Examples/FirstPerson/Assets";
			character.animations[FirstPersonCharacter::Action::IDLE] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Idle.anim");
			character.animations[FirstPersonCharacter::Action::WALK] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Walk.anim");
			character.animations[FirstPersonCharacter::Action::RUN] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Run.anim");
			character.animations[FirstPersonCharacter::Action::ONESHOT] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "OneShot.anim");
			character.animations[FirstPersonCharacter::Action::INSPECT] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Inspect.anim");
			character.animations[FirstPersonCharacter::Action::RELOAD] = MeshManager::GetInstance().LoadSkeletalAnimation(animFilepath / "Reload.anim");

			SkeletalAnimator::AnimationLayer baseLayer;
			baseLayer.animation = character.animations[FirstPersonCharacter::Action::IDLE];
			baseLayer.speed = 1.0f;
			baseLayer.weight = 1.0f;
			skeletalAnimator.AddLayer(baseLayer);
		
			character.transitionTimes[FirstPersonCharacter::Action::IDLE] = 0.25f;
			character.transitionTimes[FirstPersonCharacter::Action::WALK] = 0.25f;
			character.transitionTimes[FirstPersonCharacter::Action::RUN] = 0.25f;
			character.transitionTimes[FirstPersonCharacter::Action::ONESHOT] = 0.0f;
			character.transitionTimes[FirstPersonCharacter::Action::INSPECT] = 0.25f;
			character.transitionTimes[FirstPersonCharacter::Action::RELOAD] = 0.25f;

		}

		if (!character.joltCharacter)
		{
			JPH::CharacterSettings settings;
			settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
			settings.mLayer = ObjectLayers::DYNAMIC;
			settings.mShape = JPH::RotatedTranslatedShapeSettings(
				JPH::Vec3(0, 0, 0),
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

		character.actionCoolDownTimer = glm::max(character.actionCoolDownTimer - deltaTime, 0.0f);

		glm::vec3 direction{};

		if (character.actionCoolDownTimer <= 0.01f)
		{
			FirstPersonCharacter::Action currentAction = FirstPersonCharacter::Action::IDLE;
			FirstPersonCharacter::Action movingAction = input.IsKeyDown(KeyCode::KEY_LEFT_SHIFT) ? FirstPersonCharacter::Action::RUN : FirstPersonCharacter::Action::WALK;

			if (input.IsKeyDown(KeyCode::KEY_A))
			{
				direction += -transform.GetRight();
				currentAction = movingAction;
			}
			if (input.IsKeyDown(KeyCode::KEY_D))
			{
				direction += transform.GetRight();
				currentAction = movingAction;
			}
			if (input.IsKeyDown(KeyCode::KEY_W))
			{
				direction += transform.GetForward();
				currentAction = movingAction;
			}
			if (input.IsKeyDown(KeyCode::KEY_S))
			{
				direction += transform.GetBack();
				currentAction = movingAction;
			}

			if (input.IsKeyPressed(KeyCode::KEY_I))
			{
				currentAction = FirstPersonCharacter::Action::INSPECT;
				character.actionCoolDownTimer = character.animations[currentAction]->GetDuration();
			}

			if (input.IsKeyPressed(KeyCode::KEY_R))
			{
				currentAction = FirstPersonCharacter::Action::RELOAD;
				character.actionCoolDownTimer = character.animations[currentAction]->GetDuration();
			}

			if (input.IsMousePressed(KeyCode::MOUSE_BUTTON_1))
			{
				currentAction = FirstPersonCharacter::Action::ONESHOT;
				character.actionCoolDownTimer = character.animations[currentAction]->GetDuration();
			}

			if (character.action != currentAction)
			{
				const float transitionTime = character.action == FirstPersonCharacter::Action::ONESHOT ? 0.0f : character.transitionTimes[character.action];
				character.action = currentAction;
				skeletalAnimator.CrossfadeLayer(0, character.animations[character.action], transitionTime);
			}
		}

		float characterSpeed = character.action == FirstPersonCharacter::Action::RUN ? character.speed * 2.0f : character.speed;

		bool jump = input.IsKeyPressed(KeyCode::SPACE);
		HandleInputFirstPerson(character.joltCharacter.get(), characterSpeed, GlmVec3ToJoltVec3(direction).NormalizedOr({}), jump, deltaTime);
	}
}

void FirstPersonCharacterSystem::OnPrePhysicsUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
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

		if (!input.IsMousePressed(KeyCode::MOUSE_BUTTON_1))
		{
			return;
		}

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
			joltPhysicsSystem.GetBodyInterface().AddForce(result.mBodyID, direction, point, JPH::EActivation::Activate);
			
			const entt::entity handle = scene->GetPhysicsSystem()->GetEntity(result.mBodyID);
			if (Enemy* enemy = scene->GetRegistry().try_get<Enemy>(handle))
			{
				enemy->health -= 30.0f;
				if (enemy->health <= 0.0f)
				{
					auto entity = scene->GetRegistry().get<Transform>(handle).GetEntity();
					scene->DeleteEntity(entity);
				}
			}
		}
	}
}

void FirstPersonCharacterSystem::OnPostPhysicsUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
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
