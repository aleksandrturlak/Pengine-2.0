#include "MaterialManager.h"

#include "Logger.h"
#include "BindlessUniformWriter.h"

#include "../Graphics/ShaderModuleManager.h"

using namespace Pengine;

MaterialManager& MaterialManager::GetInstance()
{
	static MaterialManager materialManager;
	return materialManager;
}

std::shared_ptr<Material> MaterialManager::LoadMaterial(const std::filesystem::path& filepath)
{
	if (std::shared_ptr<Material> material = GetMaterial(filepath))
	{
		return material;
	}
	else
	{
		material = Material::Load(filepath);
		if (!material)
		{
			FATAL_ERROR(filepath.string() + ":There is no such material!");
		}

		std::lock_guard<std::mutex> lock(m_MutexMaterial);
		m_MaterialsByFilepath.emplace(filepath, material);

		return material;
	}
}

std::shared_ptr<BaseMaterial> MaterialManager::LoadBaseMaterial(const std::filesystem::path& filepath)
{
	if (std::shared_ptr<BaseMaterial> baseMaterial = GetBaseMaterial(filepath))
	{
		return baseMaterial;
	}
	else
	{
		baseMaterial = BaseMaterial::Load(filepath);
		if (!baseMaterial)
		{
			FATAL_ERROR(filepath.string() + ":There is no such base material!");
		}

		std::lock_guard<std::mutex> lock(m_MutexBaseMaterial);
		m_BaseMaterialsByFilepath.emplace(filepath, baseMaterial);

		return baseMaterial;
	}
}

std::shared_ptr<Material> MaterialManager::GetMaterial(const std::filesystem::path& filepath) const
{
	std::lock_guard<std::mutex> lock(m_MutexMaterial);
	if (const auto materialByFilepath = m_MaterialsByFilepath.find(filepath);
		materialByFilepath != m_MaterialsByFilepath.end())
	{
		return materialByFilepath->second;
	}

	return nullptr;
}

std::shared_ptr<BaseMaterial> MaterialManager::GetBaseMaterial(const std::filesystem::path& filepath) const
{
	std::lock_guard<std::mutex> lock(m_MutexBaseMaterial);
	if (const auto baseMaterialByFilepath = m_BaseMaterialsByFilepath.find(filepath);
		baseMaterialByFilepath != m_BaseMaterialsByFilepath.end())
	{
		return baseMaterialByFilepath->second;
	}

	return nullptr;
}

std::shared_ptr<Material> MaterialManager::Clone(
	const std::string& name,
	const std::filesystem::path& filepath,
	const std::shared_ptr<Material>& material)
{
	std::shared_ptr<Material> clonedMaterial = Material::Clone(name, filepath, material);

	std::lock_guard<std::mutex> lock(m_MutexMaterial);
	m_MaterialsByFilepath[filepath] = clonedMaterial;

	return clonedMaterial;
}

void MaterialManager::DeleteMaterial(std::shared_ptr<Material>& material)
{
	std::lock_guard<std::mutex> lock(m_MutexMaterial);
	if (material.use_count() == 2)
	{
		BindlessUniformWriter::GetInstance().UnBindMaterial(material);
		m_MaterialsByFilepath.erase(material->GetFilepath());
	}

	material = nullptr;
}

void MaterialManager::DeleteBaseMaterial(std::shared_ptr<BaseMaterial>& baseMaterial)
{
	std::lock_guard<std::mutex> lock(m_MutexBaseMaterial);
	if (baseMaterial.use_count() == 2)
	{
		m_BaseMaterialsByFilepath.erase(baseMaterial->GetFilepath());
	}

	baseMaterial = nullptr;
}

void MaterialManager::Reload(Category category)
{
	if (category == Category::BaseMaterial || category == Category::All)
	{
		for (const auto& [filepath, baseMaterial] : m_BaseMaterialsByFilepath)
		{
			BaseMaterial::Reload(baseMaterial);
		}
	}
	
	if (category == Category::Material || category == Category::All)
	{
		for (const auto& [filepath, material] : m_MaterialsByFilepath)
		{
			Material::Reload(material, false);
		}
	}
}

void MaterialManager::SaveAll()
{
	for (const auto& [filepath, material] : m_MaterialsByFilepath)
	{
		Material::Save(material);
	}
}

void MaterialManager::ShutDown()
{
	ShaderModuleManager::GetInstance().ShutDown();

	m_MaterialsByFilepath.clear();
	m_BaseMaterialsByFilepath.clear();
}

#include "FileFormatNames.h"

void MaterialManager::ManipulateOnAllMaterialsDebug()
{
	for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path()))
	{
		if (!entry.is_directory())
		{
			if (FileFormats::Mat() == Utils::GetFileFormat(entry.path()))
			{
				auto material = LoadMaterial(Utils::GetShortFilepath(entry.path()));
				// User code ...
			}
		}
	}
}
