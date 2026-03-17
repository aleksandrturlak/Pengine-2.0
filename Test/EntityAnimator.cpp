#include <gtest/gtest.h>

#include "Core/SceneManager.h"
#include "Components/EntityAnimator.h"
#include "Core/Logger.h"

using namespace Pengine;

// ─── Keyframe ────────────────────────────────────────────────────────────────

TEST(Keyframe, Defaults)
{
	EntityAnimator::Keyframe kf;
	kf.time = 0.0f;
	kf.translation = glm::vec3(0.0f);
	kf.rotation = glm::vec3(0.0f);
	kf.scale = glm::vec3(1.0f);

	EXPECT_NEAR(kf.time, 0.0f, 1e-5f);
	EXPECT_FALSE(kf.selected);
	EXPECT_EQ(kf.interpType, EntityAnimator::Keyframe::LINEAR);
}

TEST(Keyframe, InterpolationTypes)
{
	EntityAnimator::Keyframe kf;
	kf.interpType = EntityAnimator::Keyframe::BEZIER;
	EXPECT_EQ(kf.interpType, EntityAnimator::Keyframe::BEZIER);

	kf.interpType = EntityAnimator::Keyframe::STEP;
	EXPECT_EQ(kf.interpType, EntityAnimator::Keyframe::STEP);

	kf.interpType = EntityAnimator::Keyframe::LINEAR;
	EXPECT_EQ(kf.interpType, EntityAnimator::Keyframe::LINEAR);
}

// ─── AnimationTrack ──────────────────────────────────────────────────────────

TEST(AnimationTrack, Construct)
{
	EntityAnimator::AnimationTrack track("Run", "animations/run.anim");

	EXPECT_EQ(track.GetName(), "Run");
	EXPECT_EQ(track.GetFilepath(), std::filesystem::path("animations/run.anim"));
	EXPECT_TRUE(track.keyframes.empty());
	EXPECT_TRUE(track.visible);
}

TEST(AnimationTrack, AddKeyframes)
{
	EntityAnimator::AnimationTrack track("Walk", "animations/walk.anim");

	EntityAnimator::Keyframe kf0;
	kf0.time = 0.0f;
	kf0.translation = glm::vec3(0.0f);
	kf0.scale = glm::vec3(1.0f);

	EntityAnimator::Keyframe kf1;
	kf1.time = 0.5f;
	kf1.translation = glm::vec3(1.0f, 0.0f, 0.0f);
	kf1.scale = glm::vec3(1.0f);
	kf1.interpType = EntityAnimator::Keyframe::BEZIER;

	EntityAnimator::Keyframe kf2;
	kf2.time = 1.0f;
	kf2.translation = glm::vec3(2.0f, 0.0f, 0.0f);
	kf2.scale = glm::vec3(1.0f);

	track.keyframes.push_back(kf0);
	track.keyframes.push_back(kf1);
	track.keyframes.push_back(kf2);

	EXPECT_EQ(track.keyframes.size(), 3u);
	EXPECT_NEAR(track.keyframes[1].time, 0.5f, 1e-5f);
	EXPECT_EQ(track.keyframes[1].translation, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(track.keyframes[1].interpType, EntityAnimator::Keyframe::BEZIER);
}

TEST(AnimationTrack, Visibility)
{
	EntityAnimator::AnimationTrack track("Idle", "animations/idle.anim");
	EXPECT_TRUE(track.visible);

	track.visible = false;
	EXPECT_FALSE(track.visible);
}

// ─── EntityAnimator component ────────────────────────────────────────────────

TEST(EntityAnimator, Defaults)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Animated");
		EntityAnimator& animator = entity->AddComponent<EntityAnimator>();

		EXPECT_NEAR(animator.time, 0.0f, 1e-5f);
		EXPECT_NEAR(animator.speed, 1.0f, 1e-5f);
		EXPECT_TRUE(animator.isPlaying);
		EXPECT_TRUE(animator.isLoop);
		EXPECT_FALSE(animator.animationTrack.has_value());

		entity->RemoveComponent<EntityAnimator>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EntityAnimator, AssignTrack)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Animated");
		EntityAnimator& animator = entity->AddComponent<EntityAnimator>();

		animator.animationTrack = EntityAnimator::AnimationTrack("Jump", "animations/jump.anim");
		EXPECT_TRUE(animator.animationTrack.has_value());
		EXPECT_EQ(animator.animationTrack->GetName(), "Jump");

		entity->RemoveComponent<EntityAnimator>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EntityAnimator, PlaybackControl)
{
	try
	{
		std::shared_ptr<Scene> scene = SceneManager::GetInstance().Create("Scene", "Main");
		std::shared_ptr<Entity> entity = scene->CreateEntity("Animated");
		EntityAnimator& animator = entity->AddComponent<EntityAnimator>();

		animator.isPlaying = false;
		EXPECT_FALSE(animator.isPlaying);

		animator.speed = 2.0f;
		EXPECT_NEAR(animator.speed, 2.0f, 1e-5f);

		animator.isLoop = false;
		EXPECT_FALSE(animator.isLoop);

		animator.time = 0.75f;
		EXPECT_NEAR(animator.time, 0.75f, 1e-5f);

		entity->RemoveComponent<EntityAnimator>();
		SceneManager::GetInstance().Delete(scene);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}
