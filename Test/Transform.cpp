#include <gtest/gtest.h>

#include "Core/SceneManager.h"
#include "Components/Transform.h"
#include "Core/Logger.h"

using namespace Pengine;

TEST(Transform, Create)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		EXPECT_TRUE(entity);
		if (entity)
		{
			Transform& transform = entity->AddComponent<Transform>(entity);

			EXPECT_TRUE(entity->HasComponent<Transform>());

			EXPECT_TRUE(transform.GetPosition() == glm::vec3(0.0f));
			EXPECT_TRUE(transform.GetRotation() == glm::vec3(0.0f));
			EXPECT_TRUE(transform.GetScale() == glm::vec3(1.0f));

			EXPECT_TRUE(transform.GetForward() == glm::vec3(0.0f, 0.0f, -1.0f));

			transform.Translate(glm::vec3(3.0f, 2.0f, 1.0f));
			transform.Rotate(glm::vec3(90.0f, 0.0f, 0.0f));
			transform.Scale(glm::vec3(3.0f, 2.0f, 1.0f));

			EXPECT_TRUE(transform.GetPosition() == glm::vec3(3.0f, 2.0f, 1.0f));
			EXPECT_TRUE(transform.GetRotation() == glm::vec3(90.0f, 0.0f, 0.0f));
			EXPECT_TRUE(transform.GetScale() == glm::vec3(3.0f, 2.0f, 1.0f));

			entity->RemoveComponent<Transform>();

			EXPECT_TRUE(!entity->HasComponent<Transform>());
		}

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, Callbacks)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		int translationCount = 0;
		int rotationCount = 0;
		int scaleCount = 0;

		transform.SetOnTranslationCallback("test", [&]() { translationCount++; });
		transform.SetOnRotationCallback("test", [&]() { rotationCount++; });
		transform.SetOnScaleCallback("test", [&]() { scaleCount++; });

		transform.Translate(glm::vec3(1.0f));
		transform.Rotate(glm::vec3(10.0f, 0.0f, 0.0f));
		transform.Scale(glm::vec3(2.0f));

		EXPECT_EQ(translationCount, 1);
		EXPECT_EQ(rotationCount, 1);
		EXPECT_EQ(scaleCount, 1);

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, CopyConstructor)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& original = entity->AddComponent<Transform>(entity);

		original.Translate(glm::vec3(1.0f, 2.0f, 3.0f));
		original.Rotate(glm::vec3(45.0f, 0.0f, 0.0f));
		original.Scale(glm::vec3(2.0f, 2.0f, 2.0f));

		Transform copy(original);
		EXPECT_TRUE(copy.GetPosition(Transform::System::LOCAL) == original.GetPosition(Transform::System::LOCAL));
		EXPECT_TRUE(copy.GetRotation(Transform::System::LOCAL) == original.GetRotation(Transform::System::LOCAL));
		EXPECT_TRUE(copy.GetScale(Transform::System::LOCAL) == original.GetScale(Transform::System::LOCAL));

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, LocalVsGlobal)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> parent = scene->CreateEntity("Parent");
		std::shared_ptr<Entity> child = scene->CreateEntity("Child");

		Transform& parentTransform = parent->AddComponent<Transform>(parent);
		Transform& childTransform = child->AddComponent<Transform>(child);

		parentTransform.Translate(glm::vec3(10.0f, 0.0f, 0.0f));
		parent->AddChild(child);
		childTransform.Translate(glm::vec3(5.0f, 0.0f, 0.0f));

		const glm::vec3 localPos = childTransform.GetPosition(Transform::System::LOCAL);
		const glm::vec3 globalPos = childTransform.GetPosition(Transform::System::GLOBAL);

		EXPECT_TRUE(localPos == glm::vec3(5.0f, 0.0f, 0.0f));
		EXPECT_TRUE(globalPos == glm::vec3(15.0f, 0.0f, 0.0f));

		parent->RemoveChild(child);
		parent->RemoveComponent<Transform>();
		child->RemoveComponent<Transform>();
		scene->DeleteEntity(parent);
		scene->DeleteEntity(child);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, DirectionVectors)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		// Default orientation: forward -Z, up +Y, right +X
		EXPECT_NEAR(transform.GetForward().x, 0.0f, 1e-5f);
		EXPECT_NEAR(transform.GetForward().y, 0.0f, 1e-5f);
		EXPECT_NEAR(transform.GetForward().z, -1.0f, 1e-5f);

		EXPECT_NEAR(transform.GetUp().x, 0.0f, 1e-5f);
		EXPECT_NEAR(transform.GetUp().y, 1.0f, 1e-5f);
		EXPECT_NEAR(transform.GetUp().z, 0.0f, 1e-5f);

		EXPECT_NEAR(transform.GetRight().x, 1.0f, 1e-5f);
		EXPECT_NEAR(transform.GetRight().y, 0.0f, 1e-5f);
		EXPECT_NEAR(transform.GetRight().z, 0.0f, 1e-5f);

		// Back is opposite of forward
		const glm::vec3 back = transform.GetBack();
		EXPECT_NEAR(back.x, 0.0f, 1e-5f);
		EXPECT_NEAR(back.y, 0.0f, 1e-5f);
		EXPECT_NEAR(back.z, 1.0f, 1e-5f);

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, DirtyFlags)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		// After creation all flags are set
		EXPECT_TRUE(transform.IsDirty() & Transform::DirtyFlagBits::AllTransform);

		// Clear flags manually, then verify they are cleared
		transform.SetDirty(0);
		EXPECT_EQ(transform.IsDirty(), 0u);

		// A mutation marks the transform dirty again
		transform.Translate(glm::vec3(1.0f));
		EXPECT_TRUE(transform.IsDirty() != 0u);

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, RemoveCallback)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		int count = 0;
		transform.SetOnTranslationCallback("cb", [&]() { count++; });
		transform.Translate(glm::vec3(1.0f));
		EXPECT_EQ(count, 1);

		transform.RemoveOnTranslationCallback("cb");
		transform.Translate(glm::vec3(1.0f));
		EXPECT_EQ(count, 1); // should not fire again

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, ClearCallbacks)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		int count = 0;
		transform.SetOnTranslationCallback("a", [&]() { count++; });
		transform.SetOnTranslationCallback("b", [&]() { count++; });
		transform.SetOnRotationCallback("a", [&]() { count++; });
		transform.SetOnScaleCallback("a", [&]() { count++; });

		transform.ClearOnTranslationCallbacks();
		transform.ClearOnRotationCallbacks();
		transform.ClearOnScaleCallbacks();

		transform.Translate(glm::vec3(1.0f));
		transform.Rotate(glm::vec3(10.0f, 0.0f, 0.0f));
		transform.Scale(glm::vec3(2.0f));

		EXPECT_EQ(count, 0);

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, FollowOwner)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		EXPECT_TRUE(transform.GetFollorOwner());

		transform.SetFollowOwner(false);
		EXPECT_FALSE(transform.GetFollorOwner());

		transform.SetFollowOwner(true);
		EXPECT_TRUE(transform.GetFollorOwner());

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, Copyable)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		EXPECT_TRUE(transform.IsCopyable());

		transform.SetCopyable(false);
		EXPECT_FALSE(transform.IsCopyable());

		transform.SetCopyable(true);
		EXPECT_TRUE(transform.IsCopyable());

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, GetTransformMatrix)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		transform.Translate(glm::vec3(3.0f, 0.0f, 0.0f));
		transform.Scale(glm::vec3(2.0f));

		const glm::mat4& mat = transform.GetTransform(Transform::System::LOCAL);

		// Translation is in the last column
		EXPECT_NEAR(mat[3][0], 3.0f, 1e-5f);
		EXPECT_NEAR(mat[3][1], 0.0f, 1e-5f);
		EXPECT_NEAR(mat[3][2], 0.0f, 1e-5f);

		// Uniform scale 2 on diagonal
		EXPECT_NEAR(mat[0][0], 2.0f, 1e-5f);
		EXPECT_NEAR(mat[1][1], 2.0f, 1e-5f);
		EXPECT_NEAR(mat[2][2], 2.0f, 1e-5f);

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Transform, MultipleCallbacksSameEvent)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		Transform& transform = entity->AddComponent<Transform>(entity);

		int count = 0;
		transform.SetOnTranslationCallback("first", [&]() { count++; });
		transform.SetOnTranslationCallback("second", [&]() { count++; });
		transform.SetOnTranslationCallback("third", [&]() { count++; });

		transform.Translate(glm::vec3(1.0f));
		EXPECT_EQ(count, 3);

		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}
