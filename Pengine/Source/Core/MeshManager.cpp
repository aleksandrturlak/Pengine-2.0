#include "MeshManager.h"

#include "BindlessUniformWriter.h"
#include "TextureManager.h"
#include "MaterialManager.h"
#include "Logger.h"
#include "Serializer.h"
#include "Profiler.h"

using namespace Pengine;

MeshManager& MeshManager::GetInstance()
{
	static MeshManager meshManager;
	return meshManager;
}

std::shared_ptr<Mesh> MeshManager::CreateMesh(Mesh::CreateInfo& createInfo)
{
	PROFILER_SCOPE(__FUNCTION__);

	std::shared_ptr<Mesh> mesh;
	if (mesh = GetMesh(createInfo.filepath))
	{
		mesh->Reload(createInfo);
	}
	else
	{
		mesh = std::make_shared<Mesh>(createInfo);
		std::lock_guard<std::mutex> lock(m_MutexMesh);
		m_MeshesByFilepath[createInfo.filepath] = mesh;
	}

	return mesh;
}

std::shared_ptr<Mesh> MeshManager::LoadMesh(const std::filesystem::path& filepath)
{
	PROFILER_SCOPE(__FUNCTION__);

	if (std::shared_ptr<Mesh> mesh = GetMesh(filepath))
	{
		return mesh;
	}
	else
	{
		Mesh::CreateInfo createInfo(std::move(Serializer::DeserializeMesh(filepath)));
		if (createInfo.filepath.empty())
		{
			FATAL_ERROR(filepath.string() + ":There is no such mesh!");
		}

		return CreateMesh(createInfo);
	}
}

std::shared_ptr<Mesh> MeshManager::GetMesh(const std::filesystem::path& filepath) const
{
	std::lock_guard<std::mutex> lock(m_MutexMesh);
	auto meshByFilepath = m_MeshesByFilepath.find(filepath);
	if (meshByFilepath != m_MeshesByFilepath.end())
	{
		return meshByFilepath->second;
	}

	return nullptr;
}

void MeshManager::DeleteMesh(std::shared_ptr<Mesh>& mesh)
{
	std::lock_guard<std::mutex> lock(m_MutexMesh);

	if (mesh.use_count() == 2)
	{
		m_MeshesByFilepath.erase(mesh->GetFilepath());
	}

	mesh = nullptr;
}

std::shared_ptr<SkeletalAnimation> MeshManager::CreateSkeletalAnimation(SkeletalAnimation::CreateInfo& createInfo)
{
	if (std::shared_ptr<SkeletalAnimation> skeletalAnimation = GetSkeletalAnimation(createInfo.filepath))
	{
		return skeletalAnimation;
	}
	else
	{
		skeletalAnimation = std::make_shared<SkeletalAnimation>(createInfo);
		std::lock_guard<std::mutex> lock(m_MutexSkeletalAnimation);
		m_SkeletalAnimationsByFilepath[createInfo.filepath] = skeletalAnimation;

		return skeletalAnimation;
	}
}

std::shared_ptr<SkeletalAnimation> MeshManager::LoadSkeletalAnimation(const std::filesystem::path& filepath)
{
	if (std::shared_ptr<SkeletalAnimation> skeletalAnimation = GetSkeletalAnimation(filepath))
	{
		return skeletalAnimation;
	}
	else
	{
		skeletalAnimation = Serializer::DeserializeSkeletalAnimation(filepath);
		if (!skeletalAnimation)
		{
			Logger::Error(filepath.string() + ":There is no such skeletal animation!");
			return nullptr;
		}

		std::lock_guard<std::mutex> lock(m_MutexSkeletalAnimation);
		m_SkeletalAnimationsByFilepath.emplace(filepath, skeletalAnimation);

		return skeletalAnimation;
	}
}

std::shared_ptr<SkeletalAnimation> MeshManager::GetSkeletalAnimation(const std::filesystem::path& filepath) const
{
	std::lock_guard<std::mutex> lock(m_MutexSkeletalAnimation);
	auto skeletalAnimationsByFilepath = m_SkeletalAnimationsByFilepath.find(filepath);
	if (skeletalAnimationsByFilepath != m_SkeletalAnimationsByFilepath.end())
	{
		return skeletalAnimationsByFilepath->second;
	}

	return nullptr;
}

void MeshManager::DeleteSkeletalAnimation(std::shared_ptr<SkeletalAnimation>& skeletalAnimation)
{
	std::lock_guard<std::mutex> lock(m_MutexSkeletalAnimation);
	
	if (skeletalAnimation.use_count() == 2)
	{
		m_SkeletalAnimationsByFilepath.erase(skeletalAnimation->GetFilepath());
	}

	skeletalAnimation = nullptr;
}

std::shared_ptr<Skeleton> MeshManager::CreateSkeleton(Skeleton::CreateInfo& createInfo)
{
	if (std::shared_ptr<Skeleton> skeleton = GetSkeleton(createInfo.filepath))
	{
		return skeleton;
	}
	else
	{
		skeleton = std::make_shared<Skeleton>(createInfo);
		std::lock_guard<std::mutex> lock(m_MutexSkeleton);
		m_SkeletonsByFilepath[createInfo.filepath] = skeleton;

		return skeleton;
	}
}

std::shared_ptr<Skeleton> MeshManager::LoadSkeleton(const std::filesystem::path& filepath)
{
	if (std::shared_ptr<Skeleton> skeleton = GetSkeleton(filepath))
	{
		return skeleton;
	}
	else
	{
		skeleton = Serializer::DeserializeSkeleton(filepath);
		if (!skeleton)
		{
			FATAL_ERROR(filepath.string() + ":There is no such skeleton!");
		}

		std::lock_guard<std::mutex> lock(m_MutexSkeleton);
		m_SkeletonsByFilepath.emplace(filepath, skeleton);

		return skeleton;
	}
}

std::shared_ptr<Skeleton> MeshManager::GetSkeleton(const std::filesystem::path& filepath) const
{
	std::lock_guard<std::mutex> lock(m_MutexSkeleton);
	auto skeletonsByFilepath = m_SkeletonsByFilepath.find(filepath);
	if (skeletonsByFilepath != m_SkeletonsByFilepath.end())
	{
		return skeletonsByFilepath->second;
	}

	return nullptr;
}

void MeshManager::DeleteSkeleton(std::shared_ptr<Skeleton>& skeleton)
{
	std::lock_guard<std::mutex> lock(m_MutexSkeleton);

	if (skeleton.use_count() == 2)
	{
		m_SkeletonsByFilepath.erase(skeleton->GetFilepath());
	}

	skeleton = nullptr;
}

void MeshManager::ShutDown()
{
	{
		std::lock_guard<std::mutex> lock(m_MutexMesh);
		m_MeshesByFilepath.clear();
	}

	{
		std::lock_guard<std::mutex> lock(m_MutexSkeletalAnimation);
		m_SkeletalAnimationsByFilepath.clear();
	}

	{
		std::lock_guard<std::mutex> lock(m_MutexSkeleton);
		m_SkeletonsByFilepath.clear();
	}
}

#include "FileFormatNames.h"

void MeshManager::ManipulateOnAllMaterialsDebug()
{
	for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path()))
	{
		if (!entry.is_directory())
		{
			if (FileFormats::Mesh() == Utils::GetFileFormat(entry.path()))
			{
				auto mesh = LoadMesh(Utils::GetShortFilepath(entry.path()));
				Serializer::SerializeMesh(mesh->GetFilepath().parent_path(), mesh);
				// User code ...
			}
		}
	}
}
