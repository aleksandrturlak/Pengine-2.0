#include "PhysicsSystem.h"

#include "../Core/Scene.h"
#include "../Components/Transform.h"
#include "../Components/RigidBody.h"

#include "../Utils/Utils.h"

#include "../EventSystem/EventSystem.h"
#include "../EventSystem/CollisionEvent.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

using namespace Pengine;

PhysicsSystem::PhysicsSystem()
{
	JPH::RegisterDefaultAllocator();

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	m_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
	m_JobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	const uint32_t maxBodyCount = 4096;
	const uint32_t numBodyMutexCount = 0;
	const uint32_t maxBodyPairCount = 8192;
	const uint32_t maxContactConstraintCount = 8192;

	m_PhysicsSystem.Init(maxBodyCount, numBodyMutexCount, maxBodyPairCount, maxContactConstraintCount,
		m_BroadPhaseLayerInterfaceImpl,
		m_ObjectVsBroadPhaseLayerFilterImpl,
		m_ObjectLayerPairFilterImpl);

	m_PhysicsSystem.SetGravity({ 0.0f, -9.8f, 0.0f });
	m_PhysicsSystem.SetContactListener(&m_ContactListener);

	auto callback = [this](std::shared_ptr<Entity> entity)
	{
		if (!entity || !entity->HasComponent<RigidBody>())
		{
			return;
		}

		RigidBody& rigidBody = entity->GetComponent<RigidBody>();
		m_DestroyBodies.emplace_back(rigidBody.id);
		m_EntitiesByBodyId.erase(rigidBody.id);
	};
	m_RemoveCallbacks[GetTypeName<RigidBody>()] = callback;
}

PhysicsSystem::~PhysicsSystem()
{
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}

void PhysicsSystem::OnUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
{
	m_SceneRef = scene;

	UpdateBodies(scene);

	m_PhysicsSystem.GetBodyInterface().RemoveBodies(m_DestroyBodies.data(), m_DestroyBodies.size());
	m_PhysicsSystem.GetBodyInterface().DestroyBodies(m_DestroyBodies.data(), m_DestroyBodies.size());
	m_DestroyBodies.clear();

	constexpr int collisionStepCount = 1;
	m_PhysicsSystem.Update(deltaTime, collisionStepCount, m_TempAllocator.get(), m_JobSystem.get());

	const auto& view = scene->GetRegistry().view<RigidBody>();
	for (const entt::entity handle : view)
	{
		Transform& transform = scene->GetRegistry().get<Transform>(handle);
		RigidBody& rigidBody = scene->GetRegistry().get<RigidBody>(handle);

		if (rigidBody.isStatic)
		{
			continue;
		}

		JPH::BodyLockRead lock(m_PhysicsSystem.GetBodyLockInterface(), rigidBody.id);
		if (lock.Succeeded())
		{
			const JPH::Body& body = lock.GetBody();
			glm::vec3 position = JoltVec3ToGlmVec3(body.GetPosition());
			glm::vec3 rotation = glm::eulerAngles(JoltQuatToGlmQuat(body.GetRotation()));

			const std::shared_ptr<Entity> entity = transform.GetEntity();
			if (entity->HasParent())
			{
				const Transform& parentTransform = entity->GetParent()->GetComponent<Transform>();

				glm::vec3 scale;
				Utils::DecomposeTransform(
					parentTransform.GetInverseTransformMat4() * glm::translate(glm::mat4(1.0f), position) * glm::toMat4(JoltQuatToGlmQuat(body.GetRotation())),
					position,
					rotation,
					scale);
			}

			transform.Translate(position);
			transform.Rotate(rotation);

			// rigidBody.linearVelocity = JoltVec3ToGlmVec3(body.GetLinearVelocity());
			// rigidBody.angularVelocity = JoltVec3ToGlmVec3(body.GetAngularVelocity());
		}
	}

	std::vector<PendingCollision> pendingCopy;
	{
		std::lock_guard<std::mutex> lock(m_PendingCollisionMutex);
		pendingCopy.swap(m_PendingCollisions);
	}

	for (const auto& pending : pendingCopy)
	{
		const entt::entity handleA = GetEntity(pending.bodyA);
		const entt::entity handleB = GetEntity(pending.bodyB);
		if (handleA == entt::tombstone || handleB == entt::tombstone)
		{
			continue;
		}

		const std::shared_ptr<Entity> entityA = scene->GetRegistry().get<Transform>(handleA).GetEntity();
		const std::shared_ptr<Entity> entityB = scene->GetRegistry().get<Transform>(handleB).GetEntity();

		Event::Type eventType;
		switch (pending.type)
		{
		case 0: eventType = Event::Type::OnCollisionEnter;   break;
		case 1: eventType = Event::Type::OnCollisionStay;    break;
		case 2: eventType = Event::Type::OnCollisionExit;    break;
		default: continue;
		}

		auto event = std::make_shared<CollisionEvent>(entityA, entityB, eventType, this);
		EventSystem::GetInstance().SendEvent(event);
	}
}

void PhysicsSystem::UpdateBodies(std::shared_ptr<Scene> scene)
{
	std::vector<JPH::BodyID> destroyBodies;
	std::vector<JPH::BodyID> addBodies;

	const auto& view = scene->GetRegistry().view<RigidBody>();
	for (const entt::entity handle : view)
	{
		Transform& transform = scene->GetRegistry().get<Transform>(handle);
		RigidBody& rigidBody = scene->GetRegistry().get<RigidBody>(handle);

		const glm::vec3 position = transform.GetPosition();
		const glm::quat rotation = transform.GetRotation();

		if (rigidBody.isValid)
		{
			m_PhysicsSystem.GetBodyInterface().SetPositionAndRotationWhenChanged(
				rigidBody.id,
				GlmVec3ToJoltVec3(position),
				GlmQuatToJoltQuat(rotation),
				JPH::EActivation::Activate);
		}
		else
		{
			if (!rigidBody.id.IsInvalid())
			{
				destroyBodies.emplace_back(rigidBody.id);
				m_EntitiesByBodyId.erase(rigidBody.id);
			}

			JPH::ShapeSettings::ShapeResult shapeResult;
			switch (rigidBody.type)
			{
			case RigidBody::Type::Box:
			{
				JPH::BoxShapeSettings boxShapeSettings(GlmVec3ToJoltVec3(rigidBody.shape.box.halfExtents));
				shapeResult = boxShapeSettings.Create();
				break;
			}
			case RigidBody::Type::Sphere:
			{
				JPH::SphereShapeSettings sphereShapeSettings(rigidBody.shape.sphere.radius);
				shapeResult = sphereShapeSettings.Create();
				break;
			}
			case RigidBody::Type::Cylinder:
			{
				JPH::CylinderShapeSettings cylinderShapeSettings(rigidBody.shape.cylinder.halfHeight, rigidBody.shape.cylinder.radius);
				shapeResult = cylinderShapeSettings.Create();
				break;
			}
			case RigidBody::Type::Capsule:
			{
				JPH::CapsuleShapeSettings capsuleShapeSettings(rigidBody.shape.capsule.halfHeight, rigidBody.shape.capsule.radius);
				shapeResult = capsuleShapeSettings.Create();
				break;
			}
			}

			JPH::BodyCreationSettings bodySettings(
				shapeResult.Get(),
				GlmVec3ToJoltVec3(position),
				GlmQuatToJoltQuat(rotation),
				rigidBody.isStatic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic,
				rigidBody.isStatic ? ObjectLayers::STATIC : ObjectLayers::DYNAMIC
			);

			if (!rigidBody.isStatic)
			{
				bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
				bodySettings.mMassPropertiesOverride.mMass = rigidBody.mass;
			}

			bodySettings.mFriction = rigidBody.friction;
			bodySettings.mRestitution = rigidBody.restitution;
			bodySettings.mAllowSleeping = rigidBody.allowSleeping;
			// bodySettings.mLinearVelocity = GlmVec3ToJoltVec3(rigidBody.linearVelocity);
			// bodySettings.mAngularVelocity = GlmVec3ToJoltVec3(rigidBody.angularVelocity);

			JPH::Body* body = m_PhysicsSystem.GetBodyInterface().CreateBody(bodySettings);
			rigidBody.id = body->GetID();
			rigidBody.isValid = true;

			addBodies.emplace_back(rigidBody.id);

			m_EntitiesByBodyId[rigidBody.id] = handle;
		}
	}

	m_PhysicsSystem.GetBodyInterface().RemoveBodies(destroyBodies.data(), destroyBodies.size());
	m_PhysicsSystem.GetBodyInterface().DestroyBodies(destroyBodies.data(), destroyBodies.size());

	const auto state = m_PhysicsSystem.GetBodyInterface().AddBodiesPrepare(addBodies.data(), addBodies.size());
	m_PhysicsSystem.GetBodyInterface().AddBodiesFinalize(addBodies.data(), addBodies.size(), state, JPH::EActivation::Activate);
}

entt::entity PhysicsSystem::GetEntity(JPH::BodyID bodyId) const
{
	auto foundEntity = m_EntitiesByBodyId.find(bodyId);
	if (foundEntity != m_EntitiesByBodyId.end())
	{
		return foundEntity->second;
	}

	return entt::tombstone;
}

void PhysicsSystem::AddForce(const RigidBody& rb, const glm::vec3& force)
{
	if (!rb.isValid || rb.id.IsInvalid())
	{
		return;
	}
	m_PhysicsSystem.GetBodyInterface().AddForce(rb.id, GlmVec3ToJoltVec3(force), JPH::EActivation::Activate);
}

void PhysicsSystem::AddImpulse(const RigidBody& rb, const glm::vec3& impulse)
{
	if (!rb.isValid || rb.id.IsInvalid())
	{
		return;
	}
	m_PhysicsSystem.GetBodyInterface().AddImpulse(rb.id, GlmVec3ToJoltVec3(impulse));
}

void PhysicsSystem::AddTorque(const RigidBody& rb, const glm::vec3& torque)
{
	if (!rb.isValid || rb.id.IsInvalid())
	{
		return;
	}
	m_PhysicsSystem.GetBodyInterface().AddTorque(rb.id, GlmVec3ToJoltVec3(torque), JPH::EActivation::Activate);
}
