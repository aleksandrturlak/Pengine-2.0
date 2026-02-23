#include "SkeletalAnimator.h"

#include "../Graphics/Buffer.h"
#include "../Graphics/Skeleton.h"
#include "../Components/Transform.h"
#include "../Core/Logger.h"
#include "../Core/Entity.h"
#include "../Core/Scene.h"
#include "../Utils/Utils.h"

namespace Pengine
{

	std::shared_ptr<Buffer> SkeletalAnimator::CreateBoneBuffer(uint32_t boneCount)
	{
		Buffer::CreateInfo createInfo{};
		createInfo.instanceSize = sizeof(glm::mat4);
		createInfo.instanceCount = boneCount;
		createInfo.usages = { Buffer::Usage::STORAGE_BUFFER };
		createInfo.memoryType = MemoryType::CPU;
		createInfo.isMultiBuffered = true;
		return Buffer::Create(createInfo);
	}

	SkeletalAnimator::SkeletalAnimator()
	{
		m_FinalBoneMatrices.resize(100, glm::mat4(1.0f));
		m_Buffer = CreateBoneBuffer(100);
	}

	SkeletalAnimator::SkeletalAnimator(const SkeletalAnimator& skeletalAnimator)
	{
		m_CurrentTime = skeletalAnimator.GetCurrentTime();
		m_Speed = skeletalAnimator.GetSpeed();
		m_SkeletalAnimation = skeletalAnimator.GetSkeletalAnimation();
		m_NextSkeletalAnimation = skeletalAnimator.GetNextSkeletalAnimation();
		m_Skeleton = skeletalAnimator.GetSkeleton();
		m_FinalBoneMatrices.resize(100, glm::mat4(1.0f));
		m_Buffer = CreateBoneBuffer(100);
	}

	void SkeletalAnimator::UpdateAnimation(std::shared_ptr<Entity> entity, const float deltaTime, const glm::mat4& parentTransform)
	{
		if (!m_SkeletalAnimation || !m_Skeleton)
		{
			return;
		}

		if (m_BoneEntityCache.empty())
		{
			RebuildBoneEntityCache(entity);
		}

		m_CurrentTime += deltaTime * m_Speed;

		if (m_NextSkeletalAnimation)
		{
			m_NextTime += deltaTime * m_Speed;
			if (!m_IsBlending)
			{
				m_TransitionTimer += deltaTime;

				if (m_TransitionTimer >= m_TransitionTime)
				{
					m_TransitionTimer = 0.0f;
					m_TransitionTime = 0.0f;
					m_CurrentTime = m_NextTime;
					m_NextTime = 0.0f;
					m_SkeletalAnimation = m_NextSkeletalAnimation;
					m_NextSkeletalAnimation = nullptr;
					RebuildAnimationBoneCache();
				}
			}
		}

		m_CurrentTime = fmod(m_CurrentTime, m_SkeletalAnimation->GetDuration());
		if (m_NextSkeletalAnimation)
		{
			m_NextTime = fmod(m_NextTime, m_NextSkeletalAnimation->GetDuration());
		}

		for (const uint32_t rootBoneId : m_Skeleton->GetRootBoneIds())
		{
			CalculateBoneTransform(rootBoneId, parentTransform);
		}

		m_Buffer->WriteToBuffer(
			m_FinalBoneMatrices.data(),
			sizeof(glm::mat4) * m_FinalBoneMatrices.size());
		m_Buffer->Flush();
	}

	void SkeletalAnimator::SetSkeleton(std::shared_ptr<Skeleton> skeleton)
	{
		m_Skeleton = std::move(skeleton);

		if (m_Skeleton)
		{
			const uint32_t boneCount = static_cast<uint32_t>(m_Skeleton->GetBones().size());
			m_FinalBoneMatrices.assign(boneCount, glm::mat4(1.0f));
			m_Buffer = CreateBoneBuffer(boneCount);
			RebuildAnimationBoneCache();
		}
	}

	void SkeletalAnimator::SetSkeletalAnimation(std::shared_ptr<SkeletalAnimation> skeletalAnimation)
	{
		m_SkeletalAnimation = std::move(skeletalAnimation);
		m_IsBlending = false;
		RebuildAnimationBoneCache();
	}

	void SkeletalAnimator::SetNextSkeletalAnimation(std::shared_ptr<SkeletalAnimation> skeletalAnimation, float transitionTime)
	{
		m_NextSkeletalAnimation = std::move(skeletalAnimation);
		m_TransitionTime = transitionTime;
		m_TransitionTimer = 0.0f;
		m_NextTime = 0.0f;
		m_IsBlending = false;
		RebuildAnimationBoneCache();
	}

	void SkeletalAnimator::BlendSkeletalAnimations(
		std::shared_ptr<SkeletalAnimation> firstSkeletalAnimation,
		std::shared_ptr<SkeletalAnimation> secondSkeletalAnimation,
		float value)
	{
		m_SkeletalAnimation = std::move(firstSkeletalAnimation);
		m_NextSkeletalAnimation = std::move(secondSkeletalAnimation);
		m_TransitionTime = 1.0f;
		m_TransitionTimer = value;
		m_IsBlending = true;
		RebuildAnimationBoneCache();
	}

	void SkeletalAnimator::RebuildBoneEntityCache(std::shared_ptr<Entity> entity)
	{
		m_BoneEntityCache.clear();

		if (!m_Skeleton || !entity)
		{
			return;
		}

		for (const Skeleton::Bone& bone : m_Skeleton->GetBones())
		{
			if (auto boneEntity = entity->FindEntityInHierarchy(bone.name))
			{
				m_BoneEntityCache[bone.id] = boneEntity;
			}
		}
	}

	void SkeletalAnimator::RebuildAnimationBoneCache()
	{
		if (!m_Skeleton)
		{
			m_CurrentAnimBones.clear();
			m_NextAnimBones.clear();
			return;
		}

		const auto& bones = m_Skeleton->GetBones();
		const uint32_t boneCount = static_cast<uint32_t>(bones.size());

		m_CurrentAnimBones.assign(boneCount, nullptr);
		m_NextAnimBones.assign(boneCount, nullptr);

		if (m_SkeletalAnimation)
		{
			const auto& bonesByName = m_SkeletalAnimation->GetBonesByName();
			for (const Skeleton::Bone& bone : bones)
			{
				if (auto it = bonesByName.find(bone.name); it != bonesByName.end())
				{
					m_CurrentAnimBones[bone.id] = &it->second;
				}
			}
		}

		if (m_NextSkeletalAnimation)
		{
			const auto& bonesByName = m_NextSkeletalAnimation->GetBonesByName();
			for (const Skeleton::Bone& bone : bones)
			{
				if (auto it = bonesByName.find(bone.name); it != bonesByName.end())
				{
					m_NextAnimBones[bone.id] = &it->second;
				}
			}
		}
	}

	void SkeletalAnimator::CalculateBoneTransform(const uint32_t boneId, const glm::mat4& parentTransform)
	{
		const Skeleton::Bone& node = m_Skeleton->GetBones()[boneId];

		glm::vec3 currentPosition = glm::vec3(0.0f);
		glm::quat currentRotation = glm::quat(glm::vec3(0.0f));
		glm::vec3 currentScale = glm::vec3(1.0f);

		if (const SkeletalAnimation::Bone* bone = m_CurrentAnimBones[boneId])
		{
			bone->Update(m_CurrentTime, currentPosition, currentRotation, currentScale);
		}

		if (const SkeletalAnimation::Bone* nextBone = m_NextAnimBones[boneId])
		{
			glm::vec3 nextPosition;
			glm::quat nextRotation;
			glm::vec3 nextScale;
			nextBone->Update(m_NextTime, nextPosition, nextRotation, nextScale);

			const float blendFactor = m_TransitionTimer / m_TransitionTime;
			currentPosition = glm::mix(currentPosition, nextPosition, blendFactor);
			currentRotation = glm::normalize(glm::slerp(currentRotation, nextRotation, blendFactor));
			currentScale = glm::mix(currentScale, nextScale, blendFactor);
		}

		const glm::mat4 nodeTransform = glm::translate(glm::mat4(1.0f), currentPosition) * glm::toMat4(currentRotation) * glm::scale(glm::mat4(1.0f), currentScale);
		const glm::mat4 globalTransform = m_ApplySkeletonTransform
			? parentTransform * node.transform * nodeTransform
			: parentTransform * nodeTransform;

		m_FinalBoneMatrices[node.id] = globalTransform * node.offset;

		if (auto it = m_BoneEntityCache.find(node.id); it != m_BoneEntityCache.end())
		{
			if (const auto boneEntity = it->second.lock())
			{
				Transform& boneEntityTransform = boneEntity->GetComponent<Transform>();

				if (m_ApplySkeletonTransform)
				{
					glm::vec3 newPosition = glm::vec3(0.0f);
					glm::vec3 newRotation = glm::vec3(0.0f);
					glm::vec3 newScale = glm::vec3(1.0f);
					Utils::DecomposeTransform(globalTransform, newPosition, newRotation, newScale);
					boneEntityTransform.Translate(newPosition);
					boneEntityTransform.Rotate(newRotation);
					boneEntityTransform.Scale(newScale);
				}
				else
				{
					boneEntityTransform.Translate(currentPosition);
					boneEntityTransform.Rotate(glm::eulerAngles(currentRotation));
					boneEntityTransform.Scale(currentScale);
				}

				if (GetDrawDebugSkeleton() && boneEntity->HasParent())
				{
					boneEntity->GetScene()->GetVisualizer().DrawLine(
						boneEntityTransform.GetPosition(),
						boneEntity->GetParent()->GetComponent<Transform>().GetPosition(),
						{ 1.0f, 0.0f, 1.0f });
				}
			}
		}

		for (const uint32_t childId : node.childIds)
		{
			CalculateBoneTransform(m_Skeleton->GetBones()[childId].id, globalTransform);
		}
	}

}
