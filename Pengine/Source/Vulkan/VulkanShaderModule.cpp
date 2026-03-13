#include "VulkanShaderModule.h"

#include "VulkanDevice.h"
#include "VulkanPipelineUtils.h"
#include "ShaderIncluder.h"

using namespace Pengine;
using namespace Vk;

VulkanShaderModule::VulkanShaderModule(
	const std::filesystem::path& filepath,
	const Type type)
	: ShaderModule(filepath, type)
{
	Reload();
}

VulkanShaderModule::~VulkanShaderModule()
{
	GetVkDevice()->DeleteResource([shaderModule = m_ShaderModule]()
	{
		vkDestroyShaderModule(GetVkDevice()->GetDevice(), shaderModule, nullptr);
	});
}

void VulkanShaderModule::Reload(bool useCache)
{
	if (m_IsReloading.load())
	{
		m_IsReloading.wait(true);
		return;
	}

	m_IsReloading.store(true);

	struct ReloadGuard
	{
		std::atomic<bool>& flag;
		~ReloadGuard() { flag.store(false); flag.notify_all(); }
	} guard{ m_IsReloading };

	shaderc::CompileOptions options{};
	options.SetOptimizationLevel(shaderc_optimization_level_zero);
	options.SetIncluder(std::make_unique<ShaderIncluder>());

	const std::string spv = VulkanPipelineUtils::CompileShaderModule(GetFilepath(), options, GetType(), useCache);
	if (spv.empty())
	{
		return;
	}

	if (auto reflection = VulkanPipelineUtils::Reflect(GetFilepath(), GetType(), useCache))
	{
		m_Reflection = *reflection;
	}
	else
	{
		return;
	}

	if (m_ShaderModule != VK_NULL_HANDLE)
	{
		GetVkDevice()->DeleteResource([shaderModule = m_ShaderModule]()
		{
			vkDestroyShaderModule(GetVkDevice()->GetDevice(), shaderModule, nullptr);
		});
	}

	VulkanPipelineUtils::CreateShaderModule(spv, &m_ShaderModule);
}
