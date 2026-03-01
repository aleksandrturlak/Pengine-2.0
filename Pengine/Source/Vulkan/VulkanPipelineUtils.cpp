#include "VulkanPipelineUtils.h"

#include "ShaderIncluder.h"
#include "VulkanDevice.h"
#include "VulkanUniformLayout.h"
#include "VulkanFormat.h"

#include "../Core/Serializer.h"
#include "../Core/Logger.h"

using namespace Pengine;
using namespace Vk;

void VulkanPipelineUtils::CreateShaderModule(
	const std::string& code,
	VkShaderModule* shaderModule)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	if (vkCreateShaderModule(GetVkDevice()->GetDevice(), &shaderModuleCreateInfo, nullptr, shaderModule) != VK_SUCCESS)
	{
		FATAL_ERROR("Failed to create shader module!");
	}
}

std::string VulkanPipelineUtils::CompileShaderModule(
	const std::filesystem::path& filepath,
	shaderc::CompileOptions options,
	const ShaderModule::Type type,
	bool useCache,
	bool useLog)
{
	shaderc_shader_kind kind;
	switch (type)
	{
	case Pengine::ShaderModule::Type::VERTEX:
		kind = shaderc_shader_kind::shaderc_glsl_vertex_shader;
		break;
	case Pengine::ShaderModule::Type::FRAGMENT:
		kind = shaderc_shader_kind::shaderc_glsl_fragment_shader;
		break;
	case Pengine::ShaderModule::Type::GEOMETRY:
		kind = shaderc_shader_kind::shaderc_glsl_geometry_shader;
		break;
	case Pengine::ShaderModule::Type::COMPUTE:
		kind = shaderc_shader_kind::shaderc_glsl_compute_shader;
		break;
	default:
		return {};
	}

	std::string spv;

	if (useCache)
	{
		spv = Serializer::DeserializeShaderCache(filepath);
	}

	if (spv.empty())
	{
		shaderc::Compiler compiler{};

		shaderc::SpvCompilationResult module =
			compiler.CompileGlslToSpv(Utils::ReadFile(filepath), kind, filepath.string().c_str(), options);

		if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			Logger::Error(module.GetErrorMessage());
			return {};
		}

		spv = std::move(std::string((const char*)module.cbegin(), (const char*)module.cend()));

		Serializer::SerializeShaderCache(filepath, spv);

		if (useLog)
		{
			Logger::Log("Shader:" + filepath.string() + " has been compiled!", BOLDGREEN);
		}
	}
	else
	{
		if (useLog)
		{
			Logger::Log("Shader Cache:" + filepath.string() + " has been loaded!", BOLDGREEN);
		}
	}

	return spv;
}

std::optional<ShaderReflection::ReflectShaderModule> VulkanPipelineUtils::Reflect(
	const std::filesystem::path& filepath,
	ShaderModule::Type type,
	bool useCache)
{
	std::optional<ShaderReflection::ReflectShaderModule> loadedReflectShaderModule;
	
	if (useCache)
	{
		if (loadedReflectShaderModule = Serializer::DeserializeShaderModuleReflection(filepath))
		{
			return loadedReflectShaderModule;
		}
	}

	shaderc::CompileOptions options{};
	options.SetOptimizationLevel(shaderc_optimization_level_zero);
	options.SetGenerateDebugInfo();
	options.SetPreserveBindings(true);
	options.SetIncluder(std::make_unique<ShaderIncluder>());

	std::string spv = CompileShaderModule(filepath, options, type, false, false);

	SpvReflectShaderModule reflectModule{};
	SpvReflectResult result = spvReflectCreateShaderModule(spv.size(), spv.data(), &reflectModule);
	if (result != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error(std::format("Failed to get spirv reflection of {}!", filepath.string()));
		return std::nullopt;
	}

	ShaderReflection::ReflectShaderModule reflectShaderModule{};

	ReflectDescriptorSets(reflectModule, reflectShaderModule);
	ReflectInputVariables(reflectModule, reflectShaderModule);

	spvReflectDestroyShaderModule(&reflectModule);

	Serializer::SerializeShaderModuleReflection(filepath, reflectShaderModule);

	return reflectShaderModule;
}

void VulkanPipelineUtils::ReflectDescriptorSets(
	SpvReflectShaderModule& reflectModule,
	ShaderReflection::ReflectShaderModule& reflectShaderModule)
{
	uint32_t count = 0;
	SpvReflectResult result = spvReflectEnumerateDescriptorSets(&reflectModule, &count, NULL);
	if (result != SPV_REFLECT_RESULT_SUCCESS)
	{
		FATAL_ERROR("Failed to reflect to enumerate descriptor sets!");
	}

	std::vector<SpvReflectDescriptorSet*> reflectSets(count);
	result = spvReflectEnumerateDescriptorSets(&reflectModule, &count, reflectSets.data());
	if (result != SPV_REFLECT_RESULT_SUCCESS)
	{
		FATAL_ERROR("Failed to reflect to enumerate descriptor sets!");
	}

	std::function<void(ShaderReflection::ReflectVariable&, const SpvReflectBlockVariable&)> reflectStruct;

	reflectStruct = [&reflectStruct](
						ShaderReflection::ReflectVariable& variable,
						const SpvReflectBlockVariable& reflectBlock)
	{
		for (uint32_t memberIndex = 0; memberIndex < reflectBlock.member_count; memberIndex++)
		{
			const auto& member = reflectBlock.members[memberIndex];
			ShaderReflection::ReflectVariable& memberVariable = variable.variables.emplace_back();

			memberVariable.name = member.name;
			memberVariable.size = member.size;
			memberVariable.offset = member.offset;
			memberVariable.type = ShaderReflection::ReflectVariable::Type::UNDEFINED;

			// NOTE: Support only single dimensional array.
			if (member.array.dims_count == 1)
			{
				memberVariable.count = member.array.dims[0];
			}

			if (!member.type_description)
			{
				continue;
			}

			switch (member.type_description->op)
			{
			case SpvOp::SpvOpTypeStruct:
			{
				memberVariable.type = ShaderReflection::ReflectVariable::Type::STRUCT;
				reflectStruct(memberVariable, member);
				break;
			}
			case SpvOp::SpvOpTypeMatrix:
			{
				memberVariable.type = ShaderReflection::ReflectVariable::Type::MATRIX;
				break;
			}
			case SpvOp::SpvOpTypeFloat:
			{
				memberVariable.type = ShaderReflection::ReflectVariable::Type::FLOAT;
				break;
			}
			case SpvOp::SpvOpTypeInt:
			{
				memberVariable.type = ShaderReflection::ReflectVariable::Type::INT;

				if (Utils::Contains(Utils::ToLower(memberVariable.name), "texture"))
				{
					memberVariable.type = ShaderReflection::ReflectVariable::Type::TEXTURE;
				}

				break;
			}
			case SpvOp::SpvOpTypeVector:
			{
				switch (member.numeric.vector.component_count)
				{
				case 2:
				{
					memberVariable.type = ShaderReflection::ReflectVariable::Type::VEC2;
					break;
				}
				case 3:
				{
					memberVariable.type = ShaderReflection::ReflectVariable::Type::VEC3;
					break;
				}
				case 4:
				{
					memberVariable.type = ShaderReflection::ReflectVariable::Type::VEC4;
					break;
				}
				default:
					break;
				}
				break;
			}
			default:
				break;
			}

			if (member.member_count > 0 && member.array.dims_count == 1)
			{
				memberVariable.type = ShaderReflection::ReflectVariable::Type::STRUCT;
				reflectStruct(memberVariable, member);
			}
		}
	};

	for (const auto& reflectSet : reflectSets)
	{
		ShaderReflection::ReflectDescriptorSetLayout& setLayout = reflectShaderModule.setLayouts.emplace_back();
		setLayout.set = reflectSet->set;
		for (uint32_t bindingIndex = 0; bindingIndex < reflectSet->binding_count; ++bindingIndex)
		{
			const SpvReflectDescriptorBinding& reflectBinding = *(reflectSet->bindings[bindingIndex]);
			ShaderReflection::ReflectDescriptorSetBinding& binding = setLayout.bindings.emplace_back();

			if (reflectBinding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
				reflectBinding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER)
			{
				binding.name = reflectBinding.type_description->type_name;
				binding.binding = reflectBinding.binding;
				binding.type = VulkanUniformLayout::ConvertDescriptorType(static_cast<VkDescriptorType>(reflectBinding.descriptor_type));
				binding.count = 1;

				ShaderReflection::ReflectVariable& buffer = binding.buffer.emplace();
				buffer.size = reflectBinding.block.size;
				buffer.name = reflectBinding.type_description->type_name;
				buffer.offset = reflectBinding.block.offset;
				buffer.type = ShaderReflection::ReflectVariable::Type::STRUCT;

				reflectStruct(buffer, reflectBinding.block);

				// Storage Buffer always has 0 size in SPV-Reflect, so if we want to have a size,
				// so we need explicitly calculate the size of all members.
				if (binding.type == ShaderReflection::Type::STORAGE_BUFFER)
				{
					for (const auto& variable : buffer.variables)
					{
						buffer.size += variable.size;
					}
				}
			}
			else if (reflectBinding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
					 reflectBinding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				binding.name = reflectBinding.name;
				binding.binding = reflectBinding.binding;
				binding.type = VulkanUniformLayout::ConvertDescriptorType(static_cast<VkDescriptorType>(reflectBinding.descriptor_type));
				binding.count = 1;
				for (uint32_t dimIndex = 0; dimIndex < reflectBinding.array.dims_count; ++dimIndex)
				{
					// Check exactly.
					binding.count *= reflectBinding.array.dims[dimIndex];
				}
			}
			else if (reflectBinding.descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
			{
				binding.name = reflectBinding.name;
				binding.binding = reflectBinding.binding;
				binding.type = VulkanUniformLayout::ConvertDescriptorType(static_cast<VkDescriptorType>(reflectBinding.descriptor_type));
				binding.count = 1;
			}
		}
	}
}

void VulkanPipelineUtils::ReflectInputVariables(
	SpvReflectShaderModule& reflectModule,
	ShaderReflection::ReflectShaderModule& reflectShaderModule)
{
	uint32_t count = 0;
	SpvReflectResult result = spvReflectEnumerateInputVariables(&reflectModule, &count, NULL);
	if (result != SPV_REFLECT_RESULT_SUCCESS)
	{
		FATAL_ERROR("Failed to reflect to enumerate descriptor sets!");
	}

	std::vector<SpvReflectInterfaceVariable*> inputVariables(count);
	result = spvReflectEnumerateInputVariables(&reflectModule, &count, inputVariables.data());
	if (result != SPV_REFLECT_RESULT_SUCCESS)
	{
		FATAL_ERROR("Failed to reflect to enumerate descriptor sets!");
	}

	for (const auto& inputVariable : inputVariables)
	{
		if (inputVariable->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
		{
			continue;
		}

		ShaderReflection::AttributeDescription& attributeDescription = reflectShaderModule.attributeDescriptions.emplace_back();
		attributeDescription.name = inputVariable->name;
		attributeDescription.location = inputVariable->location;
		attributeDescription.format = ConvertFormat(static_cast<VkFormat>(inputVariable->format));
		attributeDescription.size = FormatSize(attributeDescription.format);
		attributeDescription.count = 1;

		// Matrix as a shader vertex attribute is split into a certain amount of vectors,
		// so we need to now how many of these vectors are there.
		if (inputVariable->type_description->op == SpvOp::SpvOpTypeMatrix)
		{
			attributeDescription.count = inputVariable->numeric.matrix.row_count;
		}
	}
}

std::map<uint32_t, std::shared_ptr<UniformLayout>> VulkanPipelineUtils::CreateDescriptorSetLayouts(
	const std::map<uint32_t, std::vector<ShaderReflection::ReflectDescriptorSetBinding>>& bindingsByDescriptorSet)
{
	std::map<uint32_t, std::shared_ptr<UniformLayout>> uniformLayoutsByDescriptorSets;
	for (const auto& [set, bindings] : bindingsByDescriptorSet)
	{
		uniformLayoutsByDescriptorSets[set] = UniformLayout::Create(bindings);
	}

	return uniformLayoutsByDescriptorSets;
}

VkShaderStageFlagBits VulkanPipelineUtils::ConvertShaderStage(ShaderModule::Type stage)
{
	switch (stage)
	{
	case Pengine::ShaderModule::Type::VERTEX:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case Pengine::ShaderModule::Type::FRAGMENT:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case Pengine::ShaderModule::Type::GEOMETRY:
		return VK_SHADER_STAGE_GEOMETRY_BIT;
	case Pengine::ShaderModule::Type::COMPUTE:
		return VK_SHADER_STAGE_COMPUTE_BIT;
	}

	FATAL_ERROR("Failed to convert shader type!");
	return {};
}

ShaderModule::Type VulkanPipelineUtils::ConvertShaderStage(VkShaderStageFlagBits stage)
{
	switch (stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:
		return Pengine::ShaderModule::Type::VERTEX;
	case VK_SHADER_STAGE_FRAGMENT_BIT:
		return Pengine::ShaderModule::Type::FRAGMENT;
	case VK_SHADER_STAGE_GEOMETRY_BIT:
		return Pengine::ShaderModule::Type::GEOMETRY;
	case VK_SHADER_STAGE_COMPUTE_BIT:
		return Pengine::ShaderModule::Type::COMPUTE;
	}

	FATAL_ERROR("Failed to convert shader type!");
	return {};
}
