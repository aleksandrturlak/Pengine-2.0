#include <gtest/gtest.h>

#include "Core/SceneManager.h"
#include "Core/Logger.h"

using namespace Pengine;

TEST(Entity, Create)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		EXPECT_TRUE(entity);
		if (entity)
		{
			EXPECT_TRUE(entity->GetName() == "GameObject");
			EXPECT_TRUE(entity->GetHandle() != entt::tombstone);
			EXPECT_TRUE(entity->GetScene() == scene);
			EXPECT_TRUE(entity->GetParent() == nullptr);
			EXPECT_TRUE(entity->GetUUID().IsValid());
		}

		EXPECT_TRUE(!scene->GetEntities().empty());
		scene->DeleteEntity(entity);
		EXPECT_TRUE(scene->GetEntities().empty());

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

TEST(Entity, AddChild)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		std::shared_ptr<Entity> child = scene->CreateEntity("Child");
		if (entity)
		{
			entity->AddChild(child);

			EXPECT_TRUE(entity->HasAsChild(child));
			EXPECT_TRUE(child->HasAsParent(entity));

			EXPECT_TRUE(entity == child->GetParent());
			EXPECT_TRUE(!entity->GetChilds().empty());
			entity->RemoveChild(child);
			EXPECT_TRUE(entity->GetChilds().empty());
			EXPECT_TRUE(child->GetParent() == nullptr);
		}

		scene->DeleteEntity(entity);
		scene->DeleteEntity(child);

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Entity, IsEnabled)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");
		std::shared_ptr<Entity> child = scene->CreateEntity("Child");
		if (entity)
		{
			entity->SetEnabled(true);
			EXPECT_TRUE(entity->IsEnabled());

			entity->SetEnabled(false);
			EXPECT_TRUE(!entity->IsEnabled());

			entity->AddChild(child);

			EXPECT_TRUE(!child->IsEnabled());

			entity->SetEnabled(true);
			EXPECT_TRUE(child->IsEnabled());

			entity->RemoveChild(child);
		}

		scene->DeleteEntity(entity);
		scene->DeleteEntity(child);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Entity, GetTopEntity)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> root = scene->CreateEntity("Root");
		std::shared_ptr<Entity> child = scene->CreateEntity("Child");
		std::shared_ptr<Entity> grandchild = scene->CreateEntity("Grandchild");

		root->AddChild(child);
		child->AddChild(grandchild);

		EXPECT_TRUE(grandchild->GetTopEntity() == root);
		EXPECT_TRUE(child->GetTopEntity() == root);
		EXPECT_TRUE(root->GetTopEntity() == root);

		child->RemoveChild(grandchild);
		root->RemoveChild(child);
		scene->DeleteEntity(root);
		scene->DeleteEntity(child);
		scene->DeleteEntity(grandchild);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Entity, FindEntityInHierarchy)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> root = scene->CreateEntity("Root");
		std::shared_ptr<Entity> child = scene->CreateEntity("Child");
		std::shared_ptr<Entity> grandchild = scene->CreateEntity("Grandchild");

		root->AddChild(child);
		child->AddChild(grandchild);

		std::shared_ptr<Entity> found = root->FindEntityInHierarchy("Grandchild");
		EXPECT_TRUE(found != nullptr);
		EXPECT_TRUE(found == grandchild);

		std::shared_ptr<Entity> missing = root->FindEntityInHierarchy("DoesNotExist");
		EXPECT_TRUE(missing == nullptr);

		child->RemoveChild(grandchild);
		root->RemoveChild(child);
		scene->DeleteEntity(root);
		scene->DeleteEntity(child);
		scene->DeleteEntity(grandchild);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Entity, HasAsChildRecursive)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> root = scene->CreateEntity("Root");
		std::shared_ptr<Entity> child = scene->CreateEntity("Child");
		std::shared_ptr<Entity> grandchild = scene->CreateEntity("Grandchild");

		root->AddChild(child);
		child->AddChild(grandchild);

		EXPECT_TRUE(root->HasAsChild(grandchild, true));
		EXPECT_FALSE(root->HasAsChild(grandchild, false));

		child->RemoveChild(grandchild);
		root->RemoveChild(child);
		scene->DeleteEntity(root);
		scene->DeleteEntity(child);
		scene->DeleteEntity(grandchild);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Entity, SetName)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("OldName");
		EXPECT_TRUE(entity->GetName() == "OldName");

		entity->SetName("NewName");
		EXPECT_TRUE(entity->GetName() == "NewName");

		scene->DeleteEntity(entity);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Entity, PrefabState)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("GameObject");

		EXPECT_FALSE(entity->IsPrefab());

		UUID validUUID;
		entity->SetPrefabFilepathUUID(validUUID);
		EXPECT_TRUE(entity->IsPrefab());
		EXPECT_TRUE(entity->GetPrefabFilepathUUID() == validUUID);

		scene->DeleteEntity(entity);
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}