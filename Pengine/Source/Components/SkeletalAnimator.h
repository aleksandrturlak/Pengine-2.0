#pragma once

#include "../Core/Core.h"
#include "../Graphics/SkeletalAnimation.h"

namespace Pengine
{

	class Skeleton;
	class UniformWriter;
	class Buffer;
	class Entity;

	class PENGINE_API SkeletalAnimator
	{
	public:
		SkeletalAnimator();
		SkeletalAnimator(const SkeletalAnimator& skeletalAnimator);

		void UpdateAnimation(std::shared_ptr<Entity> entity, const float deltaTime, const glm::mat4& parentTransform);

		void RebuildBoneEntityCache(std::shared_ptr<Entity> entity);

		[[nodiscard]] float GetSpeed() const { return m_Speed; }

		void SetSpeed(const float speed) { m_Speed = speed; }

		[[nodiscard]] float GetCurrentTime() const { return m_CurrentTime; }

		void SetCurrentTime(const float currentTime) { m_CurrentTime = currentTime; }

		[[nodiscard]] const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

		[[nodiscard]] std::shared_ptr<Buffer> GetBuffer() const { return m_Buffer; }

		void SetBuffer(std::shared_ptr<Buffer> buffer) { m_Buffer = buffer; }

		[[nodiscard]] std::shared_ptr<Skeleton> GetSkeleton() const { return m_Skeleton; }

		void SetSkeleton(std::shared_ptr<Skeleton> skeleton);

		[[nodiscard]] std::shared_ptr<SkeletalAnimation> GetSkeletalAnimation() const { return m_SkeletalAnimation; }

		void SetSkeletalAnimation(std::shared_ptr<SkeletalAnimation> skeletalAnimation);

		[[nodiscard]] std::shared_ptr<SkeletalAnimation> GetNextSkeletalAnimation() const { return m_NextSkeletalAnimation; }

		void SetNextSkeletalAnimation(std::shared_ptr<SkeletalAnimation> skeletalAnimation, float transitionTime);

		void BlendSkeletalAnimations(std::shared_ptr<SkeletalAnimation> firstSkeletalAnimation, std::shared_ptr<SkeletalAnimation> secondSkeletalAnimation, float value);

		bool GetDrawDebugSkeleton() const { return m_DrawDebugSkeleton; }

		void SetDrawDebugSkeleton(bool drawDebugSkeleton) { m_DrawDebugSkeleton = drawDebugSkeleton; }

		bool GetApplySkeletonTransform() const { return m_ApplySkeletonTransform; }

		void SetApplySkeletonTransform(bool applySkeletonTransform) { m_ApplySkeletonTransform = applySkeletonTransform; }

	private:
		static std::shared_ptr<Buffer> CreateBoneBuffer(uint32_t boneCount);

		void RebuildAnimationBoneCache();

		void CalculateBoneTransform(uint32_t boneId, const glm::mat4& parentTransform);

		std::vector<glm::mat4> m_FinalBoneMatrices;

		// Per-bone pointers into animation data, indexed by skeleton bone ID.
		// Rebuilt whenever skeleton/animation assignments change.
		std::vector<const SkeletalAnimation::Bone*> m_CurrentAnimBones;
		std::vector<const SkeletalAnimation::Bone*> m_NextAnimBones;

		// Entity handles cached per bone ID to avoid per-frame hierarchy search.
		// Rebuilt by calling RebuildBoneEntityCache() after hierarchy is ready.
		std::unordered_map<uint32_t, std::weak_ptr<Entity>> m_BoneEntityCache;

		std::shared_ptr<Skeleton> m_Skeleton;
		std::shared_ptr<SkeletalAnimation> m_SkeletalAnimation;
		std::shared_ptr<SkeletalAnimation> m_NextSkeletalAnimation;

		std::shared_ptr<Buffer> m_Buffer;

		float m_TransitionTime = 0.0f;
		float m_TransitionTimer = 0.0f;
		float m_Speed = 0.0f;
		float m_CurrentTime = 0.0f;
		float m_NextTime = 0.0f;
		bool m_IsBlending = false;
		bool m_ApplySkeletonTransform = false;
		bool m_DrawDebugSkeleton = false;
	};

}
