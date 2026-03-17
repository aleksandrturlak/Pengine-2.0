#include <gtest/gtest.h>

#include "Core/SceneManager.h"
#include "Core/Logger.h"

using namespace Pengine;

TEST(Scene, Create)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		EXPECT_TRUE(scene);
		if (scene)
		{
			EXPECT_TRUE(scene->GetName() == "Scene");
			EXPECT_TRUE(scene->GetTag() == "Main");
			EXPECT_TRUE(scene->GetEntities().size() == 0);
		}
		EXPECT_TRUE(SceneManager::GetInstance().GetScenesCount() == 1);
		SceneManager::GetInstance().Delete(scene);
		EXPECT_TRUE(SceneManager::GetInstance().GetScenesCount() == 0);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Scene, FindEntityByName)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Target");

		std::shared_ptr<Entity> found = scene->FindEntityByName("Target");
		EXPECT_TRUE(found != nullptr);
		EXPECT_TRUE(found == entity);

		std::shared_ptr<Entity> missing = scene->FindEntityByName("DoesNotExist");
		EXPECT_TRUE(missing == nullptr);

		scene->DeleteEntity(entity);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Scene, FindEntityByUUID)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("UUIDTarget");
		const UUID uuid = entity->GetUUID();

		std::shared_ptr<Entity> found = scene->FindEntityByUUID(uuid);
		EXPECT_TRUE(found != nullptr);
		EXPECT_TRUE(found == entity);

		std::shared_ptr<Entity> missing = scene->FindEntityByUUID(UUID(0, 0));
		EXPECT_TRUE(missing == nullptr);

		scene->DeleteEntity(entity);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Scene, CloneEntity)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> original = scene->CreateEntity("Original");
		std::shared_ptr<Entity> clone = scene->CloneEntity(original);

		EXPECT_TRUE(clone != nullptr);
		EXPECT_TRUE(clone != original);
		EXPECT_TRUE(clone->GetName() == original->GetName());
		EXPECT_FALSE(clone->GetUUID() == original->GetUUID());
		EXPECT_EQ(scene->GetEntities().size(), 2u);

		scene->DeleteEntity(original);
		scene->DeleteEntity(clone);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Scene, Clear)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		scene->CreateEntity("A");
		scene->CreateEntity("B");
		scene->CreateEntity("C");
		EXPECT_EQ(scene->GetEntities().size(), 3u);

		scene->Clear();
		EXPECT_TRUE(scene->GetEntities().empty());

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}
