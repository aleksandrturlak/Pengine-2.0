#include <gtest/gtest.h>

#include "Core/SceneManager.h"
#include "Components/DirectionalLight.h"
#include "Components/PointLight.h"
#include "Components/SpotLight.h"
#include "Components/RigidBody.h"
#include "Core/Logger.h"

using namespace Pengine;

// ─── DirectionalLight ────────────────────────────────────────────────────────

TEST(DirectionalLight, Defaults)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Light");
		DirectionalLight& light = entity->AddComponent<DirectionalLight>();

		EXPECT_EQ(light.color, glm::vec3(1.0f, 1.0f, 1.0f));
		EXPECT_NEAR(light.intensity, 1.0f, 1e-5f);
		EXPECT_NEAR(light.ambient, 0.1f, 1e-5f);

		entity->RemoveComponent<DirectionalLight>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(DirectionalLight, Mutation)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Light");
		DirectionalLight& light = entity->AddComponent<DirectionalLight>();

		light.color = glm::vec3(0.5f, 0.8f, 1.0f);
		light.intensity = 3.0f;
		light.ambient = 0.05f;

		EXPECT_EQ(light.color, glm::vec3(0.5f, 0.8f, 1.0f));
		EXPECT_NEAR(light.intensity, 3.0f, 1e-5f);
		EXPECT_NEAR(light.ambient, 0.05f, 1e-5f);

		entity->RemoveComponent<DirectionalLight>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

// ─── PointLight ──────────────────────────────────────────────────────────────

TEST(PointLight, Defaults)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Light");
		PointLight& light = entity->AddComponent<PointLight>();

		EXPECT_EQ(light.color, glm::vec3(1.0f, 1.0f, 1.0f));
		EXPECT_NEAR(light.intensity, 1.0f, 1e-5f);
		EXPECT_NEAR(light.radius, 1.0f, 1e-5f);
		EXPECT_NEAR(light.bias, 0.01f, 1e-5f);
		EXPECT_FALSE(light.drawBoundingSphere);
		EXPECT_TRUE(light.castShadows);
		EXPECT_FALSE(light.castSSS);

		entity->RemoveComponent<PointLight>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(PointLight, Mutation)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Light");
		PointLight& light = entity->AddComponent<PointLight>();

		light.color = glm::vec3(1.0f, 0.5f, 0.0f);
		light.intensity = 5.0f;
		light.radius = 10.0f;
		light.castShadows = false;
		light.castSSS = true;
		light.drawBoundingSphere = true;

		EXPECT_EQ(light.color, glm::vec3(1.0f, 0.5f, 0.0f));
		EXPECT_NEAR(light.intensity, 5.0f, 1e-5f);
		EXPECT_NEAR(light.radius, 10.0f, 1e-5f);
		EXPECT_FALSE(light.castShadows);
		EXPECT_TRUE(light.castSSS);
		EXPECT_TRUE(light.drawBoundingSphere);

		entity->RemoveComponent<PointLight>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

// ─── SpotLight ───────────────────────────────────────────────────────────────

TEST(SpotLight, Defaults)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Light");
		SpotLight& light = entity->AddComponent<SpotLight>();

		EXPECT_EQ(light.color, glm::vec3(1.0f, 1.0f, 1.0f));
		EXPECT_NEAR(light.intensity, 1.0f, 1e-5f);
		EXPECT_NEAR(light.radius, 1.0f, 1e-5f);
		EXPECT_NEAR(light.innerCutOff, glm::radians(45.0f), 1e-5f);
		EXPECT_NEAR(light.outerCutOff, glm::radians(45.0f), 1e-5f);
		EXPECT_TRUE(light.castShadows);
		EXPECT_FALSE(light.castSSS);

		entity->RemoveComponent<SpotLight>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(SpotLight, Mutation)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Light");
		SpotLight& light = entity->AddComponent<SpotLight>();

		light.innerCutOff = glm::radians(15.0f);
		light.outerCutOff = glm::radians(30.0f);
		light.intensity = 2.5f;
		light.castShadows = false;

		EXPECT_NEAR(light.innerCutOff, glm::radians(15.0f), 1e-5f);
		EXPECT_NEAR(light.outerCutOff, glm::radians(30.0f), 1e-5f);
		EXPECT_NEAR(light.intensity, 2.5f, 1e-5f);
		EXPECT_FALSE(light.castShadows);

		entity->RemoveComponent<SpotLight>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

// ─── RigidBody ───────────────────────────────────────────────────────────────

TEST(RigidBody, Defaults)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Physics");
		RigidBody& rb = entity->AddComponent<RigidBody>();

		EXPECT_EQ(rb.type, RigidBody::Type::Box);
		EXPECT_NEAR(rb.mass, 1.0f, 1e-5f);
		EXPECT_NEAR(rb.friction, 0.5f, 1e-5f);
		EXPECT_NEAR(rb.restitution, 0.0f, 1e-5f);
		EXPECT_EQ(rb.linearVelocity, glm::vec3(0.0f));
		EXPECT_EQ(rb.angularVelocity, glm::vec3(0.0f));
		EXPECT_TRUE(rb.allowSleeping);
		EXPECT_FALSE(rb.isStatic);
		EXPECT_FALSE(rb.isValid);

		entity->RemoveComponent<RigidBody>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(RigidBody, BoxShape)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Physics");
		RigidBody& rb = entity->AddComponent<RigidBody>();

		rb.shape.box.halfExtents = glm::vec3(2.0f, 1.0f, 0.5f);
		EXPECT_EQ(rb.shape.box.halfExtents, glm::vec3(2.0f, 1.0f, 0.5f));

		entity->RemoveComponent<RigidBody>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(RigidBody, SphereShape)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Physics");
		RigidBody& rb = entity->AddComponent<RigidBody>();

		rb.type = RigidBody::Type::Sphere;
		rb.shape.sphere.radius = 3.0f;

		EXPECT_EQ(rb.type, RigidBody::Type::Sphere);
		EXPECT_NEAR(rb.shape.sphere.radius, 3.0f, 1e-5f);

		entity->RemoveComponent<RigidBody>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(RigidBody, CylinderShape)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Physics");
		RigidBody& rb = entity->AddComponent<RigidBody>();

		rb.type = RigidBody::Type::Cylinder;
		rb.shape.cylinder.halfHeight = 2.0f;
		rb.shape.cylinder.radius = 0.5f;

		EXPECT_EQ(rb.type, RigidBody::Type::Cylinder);
		EXPECT_NEAR(rb.shape.cylinder.halfHeight, 2.0f, 1e-5f);
		EXPECT_NEAR(rb.shape.cylinder.radius, 0.5f, 1e-5f);

		entity->RemoveComponent<RigidBody>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(RigidBody, StaticBody)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Physics");
		RigidBody& rb = entity->AddComponent<RigidBody>();

		rb.isStatic = true;
		rb.mass = 0.0f;

		EXPECT_TRUE(rb.isStatic);
		EXPECT_NEAR(rb.mass, 0.0f, 1e-5f);

		entity->RemoveComponent<RigidBody>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(RigidBody, CapsuleShape)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Physics");
		RigidBody& rb = entity->AddComponent<RigidBody>();

		rb.type = RigidBody::Type::Capsule;
		rb.shape.capsule.halfHeight = 1.0f;
		rb.shape.capsule.radius = 0.3f;

		EXPECT_EQ(rb.type, RigidBody::Type::Capsule);
		EXPECT_NEAR(rb.shape.capsule.halfHeight, 1.0f, 1e-5f);
		EXPECT_NEAR(rb.shape.capsule.radius, 0.3f, 1e-5f);

		entity->RemoveComponent<RigidBody>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(RigidBody, MaterialProperties)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Physics");
		RigidBody& rb = entity->AddComponent<RigidBody>();

		rb.friction = 0.8f;
		rb.restitution = 0.4f;

		EXPECT_NEAR(rb.friction, 0.8f, 1e-5f);
		EXPECT_NEAR(rb.restitution, 0.4f, 1e-5f);

		entity->RemoveComponent<RigidBody>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(RigidBody, VelocityFields)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Physics");
		RigidBody& rb = entity->AddComponent<RigidBody>();

		rb.linearVelocity = glm::vec3(1.0f, 2.0f, 3.0f);
		rb.angularVelocity = glm::vec3(0.1f, 0.2f, 0.3f);

		EXPECT_EQ(rb.linearVelocity, glm::vec3(1.0f, 2.0f, 3.0f));
		EXPECT_EQ(rb.angularVelocity, glm::vec3(0.1f, 0.2f, 0.3f));

		entity->RemoveComponent<RigidBody>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

// ─── SceneManager ────────────────────────────────────────────────────────────

TEST(SceneManager, GetByName)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("GameScene", "Play");

		std::shared_ptr<Scene> found = SceneManager::GetInstance().GetSceneByName("GameScene");
		EXPECT_TRUE(found != nullptr);
		EXPECT_TRUE(found == scene);

		std::shared_ptr<Scene> missing = SceneManager::GetInstance().GetSceneByName("NoSuchScene");
		EXPECT_TRUE(missing == nullptr);

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(SceneManager, GetByTag)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Level1", "Active");

		std::shared_ptr<Scene> found = SceneManager::GetInstance().GetSceneByTag("Active");
		EXPECT_TRUE(found != nullptr);
		EXPECT_TRUE(found == scene);

		std::shared_ptr<Scene> missing = SceneManager::GetInstance().GetSceneByTag("NoSuchTag");
		EXPECT_TRUE(missing == nullptr);

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(SceneManager, DeleteByName)
{
	try
	{
		SceneManager::GetInstance().Create("Temp", "tmp");
		EXPECT_EQ(SceneManager::GetInstance().GetScenesCount(), 1u);

		SceneManager::GetInstance().Delete("Temp");
		EXPECT_EQ(SceneManager::GetInstance().GetScenesCount(), 0u);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(SceneManager, MultipleScenesCount)
{
	try
	{
		auto s1 = SceneManager::GetInstance().Create("S1", "t1");
		auto s2 = SceneManager::GetInstance().Create("S2", "t2");
		auto s3 = SceneManager::GetInstance().Create("S3", "t3");

		EXPECT_EQ(SceneManager::GetInstance().GetScenesCount(), 3u);

		SceneManager::GetInstance().Delete(s1);
		EXPECT_EQ(SceneManager::GetInstance().GetScenesCount(), 2u);

		SceneManager::GetInstance().Delete(s2);
		SceneManager::GetInstance().Delete(s3);
		EXPECT_EQ(SceneManager::GetInstance().GetScenesCount(), 0u);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(SceneManager, SystemUpdateFlags)
{
	try
	{
		SceneManager& sm = SceneManager::GetInstance();

		sm.SetIsComponentSystemsUpdating(false);
		EXPECT_FALSE(sm.IsComponentSystemsUpdating());

		sm.SetIsComponentSystemsUpdating(true);
		EXPECT_TRUE(sm.IsComponentSystemsUpdating());

		sm.SetIsPhysicsSystemsUpdating(false);
		EXPECT_FALSE(sm.IsPhysicsSystemsUpdating());

		sm.SetIsPhysicsSystemsUpdating(true);
		EXPECT_TRUE(sm.IsPhysicsSystemsUpdating());
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}
