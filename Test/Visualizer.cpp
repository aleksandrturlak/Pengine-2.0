#include <gtest/gtest.h>

#include "Core/SceneManager.h"
#include "Core/Visualizer.h"
#include "Core/Logger.h"

using namespace Pengine;

TEST(Visualizer, InitiallyEmpty)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		Visualizer& viz = scene->GetVisualizer();

		EXPECT_TRUE(viz.GetLines().empty());

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Visualizer, DrawLine)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		Visualizer& viz = scene->GetVisualizer();

		viz.DrawLine(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		EXPECT_EQ(viz.GetLines().size(), 1u);
		const Line& line = viz.GetLines().front();
		EXPECT_EQ(line.start, glm::vec3(0.0f));
		EXPECT_EQ(line.end, glm::vec3(1.0f, 0.0f, 0.0f));
		EXPECT_EQ(line.color, glm::vec3(1.0f, 0.0f, 0.0f));

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Visualizer, DrawLineDuration)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		Visualizer& viz = scene->GetVisualizer();

		viz.DrawLine(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 2.5f);

		const Line& line = viz.GetLines().front();
		EXPECT_NEAR(line.duration, 2.5f, 1e-5f);

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Visualizer, DrawBox)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		Visualizer& viz = scene->GetVisualizer();

		viz.DrawBox(
			glm::vec3(-1.0f),
			glm::vec3(1.0f),
			glm::vec3(0.0f, 1.0f, 0.0f),
			glm::mat4(1.0f));

		// A box is made of 12 edges → 12 lines
		EXPECT_EQ(viz.GetLines().size(), 12u);

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Visualizer, MultipleDrawCalls)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		Visualizer& viz = scene->GetVisualizer();

		viz.DrawLine(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f));
		viz.DrawLine(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f));
		viz.DrawLine(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f));

		EXPECT_EQ(viz.GetLines().size(), 3u);

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(Visualizer, DrawSphere)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		Visualizer& viz = scene->GetVisualizer();

		const int segments = 8;
		viz.DrawSphere(glm::vec3(1.0f), glm::mat4(1.0f), 1.0f, segments);

		// Loop runs ring = 0..segments (inclusive).
		// Inner rings (1..segments-1): (segments-1) * segments lines for connecting within ring.
		// Previous-ring connectors (1..segments): segments * segments lines.
		// Total = segments * (2*segments - 1)
		const size_t expected = static_cast<size_t>(segments * (2 * segments - 1));
		EXPECT_EQ(viz.GetLines().size(), expected);

		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}
