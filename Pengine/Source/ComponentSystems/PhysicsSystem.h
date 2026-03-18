#pragma once

#include "../Core/Core.h"

#include "ComponentSystem.h"
#include "../Components/RigidBody.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

#include <mutex>

namespace Pengine
{

	namespace ObjectLayers
	{
		static constexpr JPH::ObjectLayer STATIC = 0;
		static constexpr JPH::ObjectLayer DYNAMIC = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
	};

	namespace BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer STATIC(0);
		static constexpr JPH::BroadPhaseLayer DYNAMIC(1);
		static constexpr uint32_t NUM_LAYERS(2);
	};

	class PENGINE_API PhysicsSystem : public ComponentSystem
	{
	public:
		PhysicsSystem();
		virtual ~PhysicsSystem() override;

		virtual void OnUpdate(const float deltaTime, std::shared_ptr<class Scene> scene) override;

		virtual std::map<std::string, std::function<void(std::shared_ptr<class Entity>)>> GetRemoveCallbacks() override { return m_RemoveCallbacks; }

		void UpdateBodies(std::shared_ptr<class Scene> scene);

		JPH::PhysicsSystem& GetInstance() { return m_PhysicsSystem; }

		JPH::TempAllocator* GetTempAllocator() { return m_TempAllocator.get(); }

		[[nodiscard]] entt::entity GetEntity(JPH::BodyID bodyId) const;

		void AddForce(const RigidBody& rb, const glm::vec3& force);
		void AddImpulse(const RigidBody& rb, const glm::vec3& impulse);
		void AddTorque(const RigidBody& rb, const glm::vec3& torque);

	private:

		JPH::PhysicsSystem m_PhysicsSystem;
		std::unique_ptr<JPH::TempAllocator> m_TempAllocator;
		std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;

		std::map<std::string, std::function<void(std::shared_ptr<class Entity>)>> m_RemoveCallbacks;
		std::vector<JPH::BodyID> m_DestroyBodies;
		std::unordered_map<JPH::BodyID, entt::entity> m_EntitiesByBodyId;

		class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
		{
		public:
			virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
			{
				switch (inLayer1)
				{
				case ObjectLayers::STATIC:
					return inLayer2 == ObjectLayers::DYNAMIC;
				case ObjectLayers::DYNAMIC:
					return true;
				default:
					assert(false);
					return false;
				}
			}
		};

		class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
		{
		public:
			BroadPhaseLayerInterfaceImpl()
			{
				m_ObjectToBroadPhase[ObjectLayers::STATIC] = BroadPhaseLayers::STATIC;
				m_ObjectToBroadPhase[ObjectLayers::DYNAMIC] = BroadPhaseLayers::DYNAMIC;
			}

			virtual uint32_t GetNumBroadPhaseLayers() const override
			{
				return BroadPhaseLayers::NUM_LAYERS;
			}

			virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
			{
				assert(inLayer < ObjectLayers::NUM_LAYERS);
				return m_ObjectToBroadPhase[inLayer];
			}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
			virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
			{
				switch ((JPH::BroadPhaseLayer::Type)inLayer)
				{
				case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::STATIC:
					return "STATIC";
				case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::DYNAMIC:
					return "DYNAMIC";
				default:
					assert(false); return "INVALID";
				}
			}
#endif

		private:
			JPH::BroadPhaseLayer m_ObjectToBroadPhase[ObjectLayers::NUM_LAYERS];
		};

		class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
		{
		public:
			virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
			{
				switch (inLayer1)
				{
				case ObjectLayers::STATIC:
					return inLayer2 == BroadPhaseLayers::DYNAMIC;
				case ObjectLayers::DYNAMIC:
					return true;
				default:
					assert(false);
					return false;
				}
			}
		};

		BroadPhaseLayerInterfaceImpl m_BroadPhaseLayerInterfaceImpl;
		ObjectVsBroadPhaseLayerFilterImpl m_ObjectVsBroadPhaseLayerFilterImpl;
		ObjectLayerPairFilterImpl m_ObjectLayerPairFilterImpl;

		struct PendingCollision
		{
			int type; // 0=Enter, 1=Stay, 2=Exit
			JPH::BodyID bodyA;
			JPH::BodyID bodyB;
		};

		class ContactListenerImpl : public JPH::ContactListener
		{
		public:
			ContactListenerImpl(PhysicsSystem* owner) : m_Owner(owner) {}

			virtual JPH::ValidateResult OnContactValidate(
				const JPH::Body&, const JPH::Body&,
				JPH::RVec3Arg, const JPH::CollideShapeResult&) override
			{
				return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
			}

			virtual void OnContactAdded(
				const JPH::Body& body1, const JPH::Body& body2,
				const JPH::ContactManifold&, JPH::ContactSettings&) override
			{
				std::lock_guard<std::mutex> lock(m_Owner->m_PendingCollisionMutex);
				m_Owner->m_PendingCollisions.push_back({ 0, body1.GetID(), body2.GetID() });
			}

			virtual void OnContactPersisted(
				const JPH::Body& body1, const JPH::Body& body2,
				const JPH::ContactManifold&, JPH::ContactSettings&) override
			{
				std::lock_guard<std::mutex> lock(m_Owner->m_PendingCollisionMutex);
				m_Owner->m_PendingCollisions.push_back({ 1, body1.GetID(), body2.GetID() });
			}

			virtual void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
			{
				std::lock_guard<std::mutex> lock(m_Owner->m_PendingCollisionMutex);
				m_Owner->m_PendingCollisions.push_back({ 2, pair.GetBody1ID(), pair.GetBody2ID() });
			}

		private:
			PhysicsSystem* m_Owner;
		};

		ContactListenerImpl m_ContactListener{ this };
		std::weak_ptr<class Scene> m_SceneRef;
		std::mutex m_PendingCollisionMutex;
		std::vector<PendingCollision> m_PendingCollisions;
	};

	inline glm::vec3 JoltVec3ToGlmVec3(const JPH::Vec3& value)
	{
		return glm::vec3(value.GetX(), value.GetY(), value.GetZ());
	}

	inline JPH::Vec3 GlmVec3ToJoltVec3(const glm::vec3& value)
	{
		return JPH::Vec3(value.x, value.y, value.z);
	}

	inline glm::quat JoltQuatToGlmQuat(const JPH::Quat& value)
	{
		return glm::quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
	}

	inline JPH::Quat GlmQuatToJoltQuat(const glm::quat& value)
	{
		return JPH::Quat(value.x, value.y, value.z, value.w);
	}

}
