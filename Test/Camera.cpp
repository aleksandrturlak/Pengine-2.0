#include <gtest/gtest.h>

#include "Core/SceneManager.h"
#include "Components/Transform.h"
#include "Components/Camera.h"
#include "Core/Logger.h"

using namespace Pengine;

TEST(Camera, DefaultValues)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("CameraEntity");
		entity->AddComponent<Transform>(entity);
		Camera& camera = entity->AddComponent<Camera>(entity);

		EXPECT_NEAR(camera.GetFov(), glm::radians(90.0f), 1e-5f);
		EXPECT_NEAR(camera.GetZNear(), 0.1f, 1e-5f);
		EXPECT_NEAR(camera.GetZFar(), 1000.0f, 1e-5f);

		entity->RemoveComponent<Camera>();
		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Camera, SetFov)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("CameraEntity");
		entity->AddComponent<Transform>(entity);
		Camera& camera = entity->AddComponent<Camera>(entity);

		camera.SetFov(glm::radians(60.0f));
		EXPECT_NEAR(camera.GetFov(), glm::radians(60.0f), 1e-5f);

		entity->RemoveComponent<Camera>();
		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Camera, SetZNearZFar)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("CameraEntity");
		entity->AddComponent<Transform>(entity);
		Camera& camera = entity->AddComponent<Camera>(entity);

		camera.SetZNear(0.01f);
		camera.SetZFar(500.0f);

		EXPECT_NEAR(camera.GetZNear(), 0.01f, 1e-5f);
		EXPECT_NEAR(camera.GetZFar(), 500.0f, 1e-5f);

		entity->RemoveComponent<Camera>();
		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Camera, SetType)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("CameraEntity");
		entity->AddComponent<Transform>(entity);
		Camera& camera = entity->AddComponent<Camera>(entity);

		camera.SetType(Camera::Type::PERSPECTIVE);
		EXPECT_EQ(camera.GetType(), Camera::Type::PERSPECTIVE);

		camera.SetType(Camera::Type::ORTHOGRAPHIC);
		EXPECT_EQ(camera.GetType(), Camera::Type::ORTHOGRAPHIC);

		entity->RemoveComponent<Camera>();
		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Camera, VisibilityMasks)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("CameraEntity");
		entity->AddComponent<Transform>(entity);
		Camera& camera = entity->AddComponent<Camera>(entity);

		camera.SetObjectVisibilityMask(0b00001111);
		camera.SetShadowVisibilityMask(0b11110000);

		EXPECT_EQ(camera.GetObjectVisibilityMask(), 0b00001111);
		EXPECT_EQ(camera.GetShadowVisibilityMask(), 0b11110000);

		entity->RemoveComponent<Camera>();
		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Camera, PassNameAndRenderTargetIndex)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("CameraEntity");
		entity->AddComponent<Transform>(entity);
		Camera& camera = entity->AddComponent<Camera>(entity);

		camera.SetPassName("CustomPass");
		EXPECT_EQ(camera.GetPassName(), "CustomPass");

		camera.SetRenderTargetIndex(2);
		EXPECT_EQ(camera.GetRenderTargetIndex(), 2);

		entity->RemoveComponent<Camera>();
		entity->RemoveComponent<Transform>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}
