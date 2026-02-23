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
		m_AccumulatedPose.resize(100);
		m_Buffer = CreateBoneBuffer(100);
	}

	SkeletalAnimator::SkeletalAnimator(const SkeletalAnimator& skeletalAnimator)
	{
		m_Skeleton = skeletalAnimator.GetSkeleton();
		m_Layers   = skeletalAnimator.GetLayers();
		m_ApplySkeletonTransform = skeletalAnimator.GetApplySkeletonTransform();
		m_DrawDebugSkeleton      = skeletalAnimator.GetDrawDebugSkeleton();

		const uint32_t boneCount = m_Skeleton
			? static_cast<uint32_t>(m_Skeleton->GetBones().size())
			: 100u;

		m_FinalBoneMatrices.assign(boneCount, glm::mat4(1.0f));
		m_AccumulatedPose.resize(boneCount);
		m_Buffer = CreateBoneBuffer(boneCount);
	}

	void SkeletalAnimator::UpdateAnimation(std::shared_ptr<Entity> entity, const float deltaTime, const glm::mat4& parentTransform)
	{
		if (m_Layers.empty() || !m_Skeleton)
		{
			return;
		}

		if (m_BoneEntityCache.empty())
		{
			RebuildBoneEntityCache(entity);
		}

		for (uint32_t i = 0; i < static_cast<uint32_t>(m_Layers.size()); ++i)
		{
			AnimationLayer& layer = m_Layers[i];
			if (!layer.animation)
			{
				continue;
			}

			layer.currentTime += deltaTime * layer.speed;

			if (layer.nextAnimation)
			{
				layer.nextTime += deltaTime * layer.speed;
				layer.transitionTimer += deltaTime;

				if (layer.transitionTimer >= layer.transitionTime)
				{
					layer.animation      = layer.nextAnimation;
					layer.currentTime    = layer.nextTime;
					layer.nextAnimation  = nullptr;
					layer.nextTime       = 0.0f;
					layer.transitionTime = 0.0f;
					layer.transitionTimer= 0.0f;
					RebuildLayerBoneCache(i);
				}
			}

			layer.currentTime = fmod(layer.currentTime, static_cast<float>(layer.animation->GetDuration()));
			if (layer.nextAnimation)
			{
				layer.nextTime = fmod(layer.nextTime, static_cast<float>(layer.nextAnimation->GetDuration()));
			}
		}

		const BonePose identityPose{};
		std::fill(m_AccumulatedPose.begin(), m_AccumulatedPose.end(), identityPose);

		for (const uint32_t rootBoneId : m_Skeleton->GetRootBoneIds())
		{
			CalculateBoneTransform(rootBoneId, parentTransform);
		}

		m_Buffer->WriteToBuffer(
			m_FinalBoneMatrices.data(),
			sizeof(glm::mat4) * m_FinalBoneMatrices.size());
		m_Buffer->Flush();
	}

	uint32_t SkeletalAnimator::AddLayer(AnimationLayer layer)
	{
		const uint32_t index = static_cast<uint32_t>(m_Layers.size());
		m_Layers.push_back(std::move(layer));
		RebuildLayerBoneCache(index);
		return index;
	}

	void SkeletalAnimator::RemoveLayer(uint32_t layerIndex)
	{
		m_Layers.erase(m_Layers.begin() + layerIndex);
	}

	void SkeletalAnimator::SetLayerAnimation(uint32_t layerIndex, std::shared_ptr<SkeletalAnimation> anim)
	{
		AnimationLayer& layer  = m_Layers[layerIndex];
		layer.animation        = std::move(anim);
		layer.nextAnimation    = nullptr;
		layer.currentTime      = 0.0f;
		layer.nextTime         = 0.0f;
		layer.transitionTime   = 0.0f;
		layer.transitionTimer  = 0.0f;
		RebuildLayerBoneCache(layerIndex);
	}

	void SkeletalAnimator::CrossfadeLayer(uint32_t layerIndex, std::shared_ptr<SkeletalAnimation> anim, float transitionTime)
	{
		AnimationLayer& layer  = m_Layers[layerIndex];
		layer.nextAnimation    = std::move(anim);
		layer.nextTime         = 0.0f;
		layer.transitionTime   = transitionTime;
		layer.transitionTimer  = 0.0f;
		RebuildLayerBoneCache(layerIndex);
	}

	void SkeletalAnimator::SetLayerWeight(uint32_t layerIndex, float weight)
	{
		m_Layers[layerIndex].weight = weight;
	}

	void SkeletalAnimator::SetLayerSpeed(uint32_t layerIndex, float speed)
	{
		m_Layers[layerIndex].speed = speed;
	}

	void SkeletalAnimator::SetSkeleton(std::shared_ptr<Skeleton> skeleton)
	{
		m_Skeleton = std::move(skeleton);

		if (m_Skeleton)
		{
			const uint32_t boneCount = static_cast<uint32_t>(m_Skeleton->GetBones().size());
			m_FinalBoneMatrices.assign(boneCount, glm::mat4(1.0f));
			m_AccumulatedPose.resize(boneCount);
			m_Buffer = CreateBoneBuffer(boneCount);
			RebuildAllLayerBoneCaches();
		}
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

	void SkeletalAnimator::RebuildLayerBoneCache(uint32_t layerIndex)
	{
		if (!m_Skeleton)
		{
			return;
		}

		AnimationLayer& layer = m_Layers[layerIndex];
		const auto& bones = m_Skeleton->GetBones();
		const uint32_t boneCount = static_cast<uint32_t>(bones.size());

		layer.animBones.assign(boneCount, nullptr);
		layer.nextAnimBones.assign(boneCount, nullptr);

		if (layer.animation)
		{
			const auto& bonesByName = layer.animation->GetBonesByName();
			for (const Skeleton::Bone& bone : bones)
			{
				if (auto it = bonesByName.find(bone.name); it != bonesByName.end())
				{
					layer.animBones[bone.id] = &it->second;
				}
			}
		}

		if (layer.nextAnimation)
		{
			const auto& bonesByName = layer.nextAnimation->GetBonesByName();
			for (const Skeleton::Bone& bone : bones)
			{
				if (auto it = bonesByName.find(bone.name); it != bonesByName.end())
				{
					layer.nextAnimBones[bone.id] = &it->second;
				}
			}
		}
	}

	void SkeletalAnimator::RebuildAllLayerBoneCaches()
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(m_Layers.size()); ++i)
		{
			RebuildLayerBoneCache(i);
		}
	}

	void SkeletalAnimator::CalculateBoneTransform(const uint32_t boneId, const glm::mat4& parentTransform)
	{
		const Skeleton::Bone& node = m_Skeleton->GetBones()[boneId];

		BonePose pose{};
		const bool isRootBone = (node.parentId == static_cast<uint32_t>(-1));

		for (const AnimationLayer& layer : m_Layers)
		{
			if (!layer.animation || layer.animBones.empty())
			{
				continue;
			}

			if (!layer.boneMask.empty())
			{
				bool inMask = false;
				for (const uint32_t maskedId : layer.boneMask)
				{
					if (maskedId == boneId) { inMask = true; break; }
				}
				if (!inMask) continue;
			}

			glm::vec3 layerPos   = glm::vec3(0.0f);
			glm::quat layerRot   = glm::quat(glm::vec3(0.0f));
			glm::vec3 layerScale = glm::vec3(1.0f);

			if (const SkeletalAnimation::Bone* animBone = layer.animBones[boneId])
			{
				animBone->Update(layer.currentTime, layerPos, layerRot, layerScale);
			}

			if (layer.nextAnimation && !layer.nextAnimBones.empty() && layer.transitionTime > 0.0f)
			{
				glm::vec3 nextPos;
				glm::quat nextRot;
				glm::vec3 nextScale;

				if (const SkeletalAnimation::Bone* nextBone = layer.nextAnimBones[boneId])
				{
					nextBone->Update(layer.nextTime, nextPos, nextRot, nextScale);
				}
				else
				{
					nextPos   = glm::vec3(0.0f);
					nextRot   = glm::quat(glm::vec3(0.0f));
					nextScale = glm::vec3(1.0f);
				}

				const float t = layer.transitionTimer / layer.transitionTime;
				layerPos   = glm::mix(layerPos, nextPos, t);
				layerRot   = glm::normalize(glm::slerp(layerRot, nextRot, t));
				layerScale = glm::mix(layerScale, nextScale, t);
			}

			if (layer.playInPlace && isRootBone)
			{
				layerPos.x = 0.0f;
				layerPos.z = 0.0f;
			}

			if (layer.blendMode == AnimationLayer::BlendMode::Override)
			{
				pose.position = glm::mix(pose.position, layerPos, layer.weight);
				pose.rotation = glm::normalize(glm::slerp(pose.rotation, layerRot, layer.weight));
				pose.scale    = glm::mix(pose.scale, layerScale, layer.weight);
			}
			else
			{
				pose.position += layerPos * layer.weight;
				pose.rotation  = glm::normalize(pose.rotation * glm::slerp(glm::quat(glm::vec3(0.0f)), layerRot, layer.weight));
				pose.scale    *= glm::mix(glm::vec3(1.0f), layerScale, layer.weight);
			}
		}

		const glm::mat4 nodeTransform =
			glm::translate(glm::mat4(1.0f), pose.position) *
			glm::toMat4(pose.rotation) *
			glm::scale(glm::mat4(1.0f), pose.scale);

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
					glm::vec3 newScale    = glm::vec3(1.0f);
					Utils::DecomposeTransform(globalTransform, newPosition, newRotation, newScale);
					boneEntityTransform.Translate(newPosition);
					boneEntityTransform.Rotate(newRotation);
					boneEntityTransform.Scale(newScale);
				}
				else
				{
					boneEntityTransform.Translate(pose.position);
					boneEntityTransform.Rotate(glm::eulerAngles(pose.rotation));
					boneEntityTransform.Scale(pose.scale);
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
