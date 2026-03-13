#include "ShaderModuleManager.h"

using namespace Pengine;

ShaderModuleManager& ShaderModuleManager::GetInstance()
{
	static ShaderModuleManager shaderModuleManager;
	return shaderModuleManager;
}

std::shared_ptr<ShaderModule> ShaderModuleManager::GetOrCreateShaderModule(
	const std::filesystem::path& filepath,
	const ShaderModule::Type type)
{
	if (std::shared_ptr<ShaderModule> shaderModule = GetShaderModule(filepath))
	{
		shaderModule->Reload();
		return shaderModule;
	}

	return CreateShaderModule(filepath, type);
}

std::shared_ptr<ShaderModule> ShaderModuleManager::CreateShaderModule(
	const std::filesystem::path& filepath,
	const ShaderModule::Type type)
{
	{
		std::lock_guard<std::mutex> lock(m_ShaderModuleMutex);
		m_CreatingShaderModules.emplace(filepath);
	}

	const std::shared_ptr<ShaderModule> shaderModule = ShaderModule::Create(filepath, type);

	std::lock_guard<std::mutex> lock(m_ShaderModuleMutex);

	if (shaderModule->IsValid())
	{
		m_ShaderModulesByFilepath[filepath] = shaderModule;
	}

	m_CreatingShaderModules.erase(filepath);
	m_CreatingShaderModuleConditionVariable.notify_all();

	return shaderModule;
}

std::shared_ptr<ShaderModule> ShaderModuleManager::GetShaderModule(const std::filesystem::path& filepath) const
{
	bool isCreating = false;
	{
		std::lock_guard<std::mutex> lock(m_ShaderModuleMutex);
		isCreating = m_CreatingShaderModules.contains(filepath);
	}

	if (isCreating)
	{
		std::unique_lock<std::mutex> lock(m_CreatingShaderModuleMutex);
		m_CreatingShaderModuleConditionVariable.wait(lock, [this, filepath]()
		{
			std::lock_guard<std::mutex> lock(m_ShaderModuleMutex);
			return !m_CreatingShaderModules.contains(filepath);
		});
	}

	{
		std::lock_guard<std::mutex> lock(m_ShaderModuleMutex);
		auto shaderModulesByFilepath = m_ShaderModulesByFilepath.find(filepath);
		if (shaderModulesByFilepath != m_ShaderModulesByFilepath.end())
		{
			return shaderModulesByFilepath->second;
		}
	}

	return nullptr;
}

void ShaderModuleManager::DeleteShaderModule(std::shared_ptr<ShaderModule>& shaderModule)
{
	if (shaderModule.use_count() == 2)
	{
		std::lock_guard<std::mutex> lock(m_ShaderModuleMutex);
		m_ShaderModulesByFilepath.erase(shaderModule->GetFilepath());
	}

	shaderModule = nullptr;
}

void ShaderModuleManager::ShutDown()
{
	std::lock_guard<std::mutex> lock(m_ShaderModuleMutex);
	m_ShaderModulesByFilepath.clear();
}
