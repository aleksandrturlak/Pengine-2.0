#pragma once

#include "../Core/Core.h"
#include "../Graphics/Pipeline.h"

#include <vulkan/vulkan.h>
#include <SPIRV-Reflect/spirv_reflect.h>

#include <shaderc/shaderc.hpp>

namespace Pengine
{

	class PENGINE_API VulkanPipelineUtils
	{
	public:
		static void CreateShaderModule(const std::string& code, VkShaderModule* shaderModule);

		static std::string CompileShaderModule(
			const std::filesystem::path& filepath,
			shaderc::CompileOptions options,
			ShaderModule::Type type,
			bool useCache = true,
			bool useLog = true);

		static std::optional<ShaderReflection::ReflectShaderModule> Reflect(
			const std::filesystem::path& filepath,
			ShaderModule::Type type,
			bool useCache = true);

		static void ReflectDescriptorSets(
			SpvReflectShaderModule& reflectModule,
			ShaderReflection::ReflectShaderModule& reflectShaderModule);

		static void ReflectInputVariables(
			SpvReflectShaderModule& reflectModule,
			ShaderReflection::ReflectShaderModule& reflectShaderModule);

		static void ReflectPushConstants(
			SpvReflectShaderModule& reflectModule,
			ShaderReflection::ReflectShaderModule& reflectShaderModule);

		static std::map<uint32_t, std::shared_ptr<UniformLayout>> CreateDescriptorSetLayouts(
			const std::map<uint32_t, std::vector<ShaderReflection::ReflectDescriptorSetBinding>>& bindingsByDescriptorSet);

		static VkShaderStageFlagBits ConvertShaderStage(ShaderModule::Type stage);

		static ShaderModule::Type ConvertShaderStage(VkShaderStageFlagBits stage);
	};

}
