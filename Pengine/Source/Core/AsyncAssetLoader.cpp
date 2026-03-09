#include "AsyncAssetLoader.h"

#include "MaterialManager.h"
#include "MeshManager.h"
#include "TextureManager.h"

#include <thread>

using namespace Pengine;

AsyncAssetLoader& AsyncAssetLoader::GetInstance()
{
	static AsyncAssetLoader asyncAssetLoader;
	return asyncAssetLoader;
}

void AsyncAssetLoader::Initialize()
{
	m_ThreadPool.Initialize(10);
}

void AsyncAssetLoader::Shutdown()
{
	m_ThreadPool.Shutdown();
}

void AsyncAssetLoader::AsyncLoadMaterial(const std::filesystem::path &filepath, std::function<void(std::weak_ptr<Material>)> &&callback)
{
	std::lock_guard<std::mutex> lock(m_MaterialMutex);
	if (!m_MaterialsLoading.count(filepath))
	{
		m_ThreadPool.EnqueueAsync([this, filepath]()
		{
			MaterialManager::GetInstance().LoadMaterial(filepath);

			std::lock_guard<std::mutex> lock(m_MaterialMutex);
			m_MaterialsLoading.erase(filepath);
			m_WaitIdleConditionalVariable.notify_all();
		});
	}

	m_MaterialsLoading.emplace(filepath);
	m_MaterialsToBeLoaded[filepath].emplace_back(std::move(callback));
}

void AsyncAssetLoader::AsyncLoadBaseMaterial(const std::filesystem::path& filepath, std::function<void(std::weak_ptr<BaseMaterial>)>&& callback)
{
	std::lock_guard<std::mutex> lock(m_BaseMaterialMutex);
	if (!m_BaseMaterialsLoading.count(filepath))
	{
		m_ThreadPool.EnqueueAsync([this, filepath]()
		{
			MaterialManager::GetInstance().LoadBaseMaterial(filepath);

			std::lock_guard<std::mutex> lock(m_BaseMaterialMutex);
			m_BaseMaterialsLoading.erase(filepath);
			m_WaitIdleConditionalVariable.notify_all();
		});
	}

	m_BaseMaterialsLoading.emplace(filepath);
	m_BaseMaterialsToBeLoaded[filepath].emplace_back(std::move(callback));
}

void AsyncAssetLoader::AsyncLoadMesh(const std::filesystem::path& filepath, std::function<void(std::weak_ptr<Mesh>)>&& callback)
{
	std::lock_guard<std::mutex> lock(m_MeshMutex);
	if (!m_MeshesLoading.count(filepath))
	{
		m_ThreadPool.EnqueueAsync([this, filepath]()
		{
			MeshManager::GetInstance().LoadMesh(filepath);

			std::lock_guard<std::mutex> lock(m_MeshMutex);
			m_MeshesLoading.erase(filepath);
			m_WaitIdleConditionalVariable.notify_all();
		});
	}

	m_MeshesLoading.emplace(filepath);
	m_MeshesToBeLoaded[filepath].emplace_back(std::move(callback));
}

void AsyncAssetLoader::AsyncLoadTexture(const std::filesystem::path& filepath, std::function<void(std::weak_ptr<class Texture>)>&& callback, bool flip)
{
	std::lock_guard<std::mutex> lock(m_TextureMutex);
	if (!m_TexturesLoading.count(filepath))
	{
		m_ThreadPool.EnqueueAsync([this, filepath, flip]()
		{
			TextureManager::GetInstance().Load(filepath, flip);

			std::lock_guard<std::mutex> lock(m_TextureMutex);
			m_TexturesLoading.erase(filepath);
			m_WaitIdleConditionalVariable.notify_all();
		});
	}

	m_TexturesLoading.emplace(filepath);
	m_TexturesToBeLoaded[filepath].emplace_back(std::move(callback));
}

std::shared_ptr<Material> AsyncAssetLoader::SyncLoadMaterial(const std::filesystem::path& filepath)
{
	bool found = false;
	{
		std::lock_guard<std::mutex> lock(m_MaterialMutex);
		found = (bool)m_MaterialsLoading.count(filepath);

		if (!found)
		{
			m_MaterialsLoading.emplace(filepath);
		}
	}

	if (found)
	{
		while (true)
		{
			std::shared_ptr<Material> material = MaterialManager::GetInstance().GetMaterial(filepath);
			if (material)
			{
				return material;
			}
			std::this_thread::yield();
		}
	}

	std::shared_ptr<Material> material = MaterialManager::GetInstance().LoadMaterial(filepath);

	{
		std::lock_guard<std::mutex> lock(m_MaterialMutex);
		m_MaterialsLoading.erase(filepath);
	}
	m_WaitIdleConditionalVariable.notify_all();

	return material;
}

std::shared_ptr<BaseMaterial> AsyncAssetLoader::SyncLoadBaseMaterial(const std::filesystem::path& filepath)
{
	bool found = false;
	{
		std::lock_guard<std::mutex> lock(m_BaseMaterialMutex);
		found = (bool)m_BaseMaterialsLoading.count(filepath);

		if (!found)
		{
			m_BaseMaterialsLoading.emplace(filepath);
		}
	}

	if (found)
	{
		while (true)
		{
			std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().GetBaseMaterial(filepath);
			if (baseMaterial)
			{
				return baseMaterial;
			}
			std::this_thread::yield();
		}
	}

	std::shared_ptr<BaseMaterial> baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(filepath);

	{
		std::lock_guard<std::mutex> lock(m_BaseMaterialMutex);
		m_BaseMaterialsLoading.erase(filepath);
	}
	m_WaitIdleConditionalVariable.notify_all();

	return baseMaterial;
}

std::shared_ptr<Mesh> AsyncAssetLoader::SyncLoadMesh(const std::filesystem::path& filepath)
{
	bool found = false;
	{
		std::lock_guard<std::mutex> lock(m_MeshMutex);
		found = (bool)m_MeshesLoading.count(filepath);

		if (!found)
		{
			m_MeshesLoading.emplace(filepath);
		}
	}

	if (found)
	{
		while (true)
		{
			std::shared_ptr<Mesh> mesh = MeshManager::GetInstance().GetMesh(filepath);
			if (mesh)
			{
				return mesh;
			}
			std::this_thread::yield();
		}
	}

	std::shared_ptr<Mesh> mesh = MeshManager::GetInstance().LoadMesh(filepath);

	{
		std::lock_guard<std::mutex> lock(m_MeshMutex);
		m_MeshesLoading.erase(filepath);
	}
	m_WaitIdleConditionalVariable.notify_all();

	return mesh;
}

std::shared_ptr<Texture> AsyncAssetLoader::SyncLoadTexture(const std::filesystem::path& filepath, bool flip)
{
	bool found = false;
	{
		std::lock_guard<std::mutex> lock(m_TextureMutex);
		found = (bool)m_TexturesLoading.count(filepath);

		if (!found)
		{
			m_TexturesLoading.emplace(filepath);
		}
	}

	if (found)
	{
		while (true)
		{
			std::shared_ptr<Texture> texture = TextureManager::GetInstance().GetTexture(filepath);
			if (texture)
			{
				return texture;
			}
			std::this_thread::yield();
		}
	}

	std::shared_ptr<Texture> texture = TextureManager::GetInstance().Load(filepath, flip);

	{
		std::lock_guard<std::mutex> lock(m_TextureMutex);
		m_TexturesLoading.erase(filepath);
	}
	m_WaitIdleConditionalVariable.notify_all();

	return texture;
}

void AsyncAssetLoader::Update()
{
	std::lock_guard<std::mutex> lock(m_UpdateMutex);

	MaterialManager& materialManager = MaterialManager::GetInstance();

	{
		std::lock_guard<std::mutex> lock(m_MaterialMutex);
		for (auto materialToBeLoaded = m_MaterialsToBeLoaded.begin(); materialToBeLoaded != m_MaterialsToBeLoaded.end(); )
		{
			const auto& filepath = materialToBeLoaded->first;
			const auto& callbacks = materialToBeLoaded->second;

			const std::weak_ptr<Material> material = materialManager.GetMaterial(filepath);
			if (!material.lock())
			{
				++materialToBeLoaded;
				continue;
			}

			for (const auto& callback : callbacks)
			{
				callback(material);
			}

			materialToBeLoaded = m_MaterialsToBeLoaded.erase(materialToBeLoaded);
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_BaseMaterialMutex);
		for (auto baseMaterialToBeLoaded = m_BaseMaterialsToBeLoaded.begin(); baseMaterialToBeLoaded != m_BaseMaterialsToBeLoaded.end(); )
		{
			const auto& filepath = baseMaterialToBeLoaded->first;
			const auto& callbacks = baseMaterialToBeLoaded->second;

			const std::weak_ptr<BaseMaterial> baseMaterial = materialManager.GetBaseMaterial(filepath);
			if (!baseMaterial.lock())
			{
				++baseMaterialToBeLoaded;
				continue;
			}

			for (const auto& callback : callbacks)
			{
				callback(baseMaterial);
			}

			baseMaterialToBeLoaded = m_BaseMaterialsToBeLoaded.erase(baseMaterialToBeLoaded);
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_MeshMutex);
		for (auto mesheToBeLoaded = m_MeshesToBeLoaded.begin(); mesheToBeLoaded != m_MeshesToBeLoaded.end(); )
		{
			const auto& filepath = mesheToBeLoaded->first;
			const auto& callbacks = mesheToBeLoaded->second;

			const std::weak_ptr<Mesh> mesh = MeshManager::GetInstance().GetMesh(filepath);
			if (!mesh.lock())
			{
				++mesheToBeLoaded;
				continue;
			}

			for (const auto& callback : callbacks)
			{
				callback(mesh);
			}

			mesheToBeLoaded = m_MeshesToBeLoaded.erase(mesheToBeLoaded);
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_TextureMutex);
		for (auto textureToBeLoaded = m_TexturesToBeLoaded.begin(); textureToBeLoaded != m_TexturesToBeLoaded.end(); )
		{
			const auto& filepath = textureToBeLoaded->first;
			const auto& callbacks = textureToBeLoaded->second;

			const std::weak_ptr<Texture> texture = TextureManager::GetInstance().GetTexture(filepath);
			if (!texture.lock())
			{
				++textureToBeLoaded;
				continue;
			}

			for (const auto& callback : callbacks)
			{
				callback(texture);
			}

			textureToBeLoaded = m_TexturesToBeLoaded.erase(textureToBeLoaded);
		}
	}
}

void AsyncAssetLoader::WaitIdle()
{
	std::unique_lock<std::mutex> lock(m_WaitMutex);
	m_WaitIdleConditionalVariable.wait(lock, [this]()
	{
		Update();

		bool empty =
			m_MaterialsToBeLoaded.empty() &&
			m_BaseMaterialsLoading.empty() &&
			m_TexturesLoading.empty() &&
			m_MeshesLoading.empty();
		return empty;
	});
}
