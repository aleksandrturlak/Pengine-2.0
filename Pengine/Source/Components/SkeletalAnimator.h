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
		struct AnimationLayer
		{
			enum class BlendMode { Override, Additive };

			std::shared_ptr<SkeletalAnimation> animation;
			std::shared_ptr<SkeletalAnimation> nextAnimation;

			float currentTime     = 0.0f;
			float nextTime        = 0.0f;
			float speed           = 1.0f;
			float weight          = 1.0f;
			float transitionTime  = 0.0f;
			float transitionTimer = 0.0f;

			BlendMode blendMode = BlendMode::Override;

			bool playInPlace = false;  // Zero root bone XZ translation (suppress horizontal root motion).

			// Empty = affect all bones. Non-empty = only these skeleton bone IDs.
			std::vector<uint32_t> boneMask;

			// Runtime cache indexed by skeleton bone ID. nullptr = not in this animation.
			std::vector<const SkeletalAnimation::Bone*> animBones;
			std::vector<const SkeletalAnimation::Bone*> nextAnimBones;
		};

		struct BonePose
		{
			glm::vec3 position = glm::vec3(0.0f);
			glm::quat rotation = glm::quat(glm::vec3(0.0f));
			glm::vec3 scale    = glm::vec3(1.0f);
		};

		SkeletalAnimator();
		SkeletalAnimator(const SkeletalAnimator& skeletalAnimator);

		void UpdateAnimation(std::shared_ptr<Entity> entity, float deltaTime, const glm::mat4& parentTransform);

		void RebuildBoneEntityCache(std::shared_ptr<Entity> entity);

		// Layer management
		uint32_t AddLayer(AnimationLayer layer);
		void RemoveLayer(uint32_t layerIndex);

		[[nodiscard]] AnimationLayer& GetLayer(uint32_t layerIndex) { return m_Layers[layerIndex]; }
		[[nodiscard]] const AnimationLayer& GetLayer(uint32_t layerIndex) const { return m_Layers[layerIndex]; }
		[[nodiscard]] const std::vector<AnimationLayer>& GetLayers() const { return m_Layers; }
		[[nodiscard]] uint32_t GetLayerCount() const { return static_cast<uint32_t>(m_Layers.size()); }

		// Per-layer shortcuts
		void SetLayerAnimation(uint32_t layerIndex, std::shared_ptr<SkeletalAnimation> anim);
		void CrossfadeLayer(uint32_t layerIndex, std::shared_ptr<SkeletalAnimation> anim, float transitionTime);
		void SetLayerWeight(uint32_t layerIndex, float weight);
		void SetLayerSpeed(uint32_t layerIndex, float speed);

		[[nodiscard]] std::shared_ptr<Skeleton> GetSkeleton() const { return m_Skeleton; }
		void SetSkeleton(std::shared_ptr<Skeleton> skeleton);

		[[nodiscard]] std::shared_ptr<Buffer> GetBuffer() const { return m_Buffer; }
		void SetBuffer(std::shared_ptr<Buffer> buffer) { m_Buffer = buffer; }

		[[nodiscard]] const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

		bool GetApplySkeletonTransform() const { return m_ApplySkeletonTransform; }
		void SetApplySkeletonTransform(bool applySkeletonTransform) { m_ApplySkeletonTransform = applySkeletonTransform; }

		bool GetDrawDebugSkeleton() const { return m_DrawDebugSkeleton; }
		void SetDrawDebugSkeleton(bool drawDebugSkeleton) { m_DrawDebugSkeleton = drawDebugSkeleton; }

	private:
		static std::shared_ptr<Buffer> CreateBoneBuffer(uint32_t boneCount);

		void RebuildLayerBoneCache(uint32_t layerIndex);
		void RebuildAllLayerBoneCaches();

		void CalculateBoneTransform(uint32_t boneId, const glm::mat4& parentTransform);

		std::vector<AnimationLayer> m_Layers;
		std::vector<BonePose>       m_AccumulatedPose;
		std::vector<glm::mat4>      m_FinalBoneMatrices;

		// Entity handles cached per bone ID to avoid per-frame hierarchy search.
		std::unordered_map<uint32_t, std::weak_ptr<Entity>> m_BoneEntityCache;

		std::shared_ptr<Skeleton> m_Skeleton;
		std::shared_ptr<Buffer>   m_Buffer;

		bool m_ApplySkeletonTransform = false;
		bool m_DrawDebugSkeleton      = false;
	};

}
