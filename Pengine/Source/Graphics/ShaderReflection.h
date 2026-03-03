#pragma once

#include "../Core/Core.h"

#include "Format.h"

namespace Pengine::ShaderReflection
{

	enum class Type
	{
		SAMPLER,
		COMBINED_IMAGE_SAMPLER,
		SAMPLED_IMAGE,
		STORAGE_IMAGE,
		UNIFORM_TEXEL_BUFFER,
		STORAGE_TEXEL_BUFFER,
		UNIFORM_BUFFER,
		STORAGE_BUFFER,
		UNIFORM_BUFFER_DYNAMIC,
		STORAGE_BUFFER_DYNAMIC,
		INPUT_ATTACHMENT,
		ACCELERATION_STRUCTURE_KHR
	};

	struct ReflectVariable
	{
		enum class Type
		{
			UNDEFINED,
			INT,
			FLOAT,
			VEC2,
			VEC3,
			VEC4,
			STRUCT,
			MATRIX,
			TEXTURE,
		};

		std::string name;
		uint32_t size = 0;
		uint32_t offset = 0;
		uint32_t count = 1;
		Type type;

		std::vector<ReflectVariable> variables;
	};

	enum class Stage
	{
		VERTEX_BIT,
		TESSELLATION_CONTROL_BIT,
		TESSELLATION_EVALUATION_BIT,
		GEOMETRY_BIT,
		FRAGMENT_BIT,
		COMPUTE_BIT,
		ALL_GRAPHICS,
		ALL,
	};

	struct ReflectDescriptorSetBinding
	{
		std::string name;
		uint32_t binding = 0;
		uint32_t count = 0;
		Type type;
		Stage stage = Stage::ALL;

		std::optional<ReflectVariable> buffer;
	};

	struct ReflectDescriptorSetLayout
	{
		uint32_t set = 0;
		std::vector<ReflectDescriptorSetBinding> bindings;
	};

	struct AttributeDescription
	{
		std::string name;
		uint32_t location;
		Format format;
		uint32_t size;
		uint32_t count;
	};

	struct PushConstantRange
	{
		uint32_t offset = 0;
		uint32_t size = 0;
	};

	struct ReflectShaderModule
	{
		std::vector<ReflectDescriptorSetLayout> setLayouts;
		std::vector<AttributeDescription> attributeDescriptions;
		std::vector<PushConstantRange> pushConstantRanges;
	};

	static ReflectVariable::Type ConvertStringToType(const std::string& type)
	{
		if (type == "int")
		{
			return ReflectVariable::Type::INT;
		}
		else if (type == "float")
		{
			return ReflectVariable::Type::FLOAT;
		}
		else if (type == "vec2")
		{
			return ReflectVariable::Type::VEC2;
		}
		else if (type == "vec3")
		{
			return ReflectVariable::Type::VEC3;
		}
		else if (type == "vec4")
		{
			return ReflectVariable::Type::VEC4;
		}
		else if (type == "matrix")
		{
			return ReflectVariable::Type::MATRIX;
		}
		else if (type == "struct")
		{
			return ReflectVariable::Type::STRUCT;
		}
		else if (type == "texture")
		{
			return ReflectVariable::Type::TEXTURE;
		}

		return ReflectVariable::Type::UNDEFINED;
	}

	static std::string ConvertTypeToString(const ReflectVariable::Type type)
	{
		if (type == ReflectVariable::Type::INT)
		{
			return "int";
		}
		else if (type == ReflectVariable::Type::FLOAT)
		{
			return "float";
		}
		else if (type == ReflectVariable::Type::VEC2)
		{
			return "vec2";
		}
		else if (type == ReflectVariable::Type::VEC3)
		{
			return "vec3";
		}
		else if (type == ReflectVariable::Type::VEC4)
		{
			return "vec4";
		}
		else if (type == ReflectVariable::Type::MATRIX)
		{
			return "matrix";
		}
		else if (type == ReflectVariable::Type::STRUCT)
		{
			return "struct";
		}
		else if (type == ReflectVariable::Type::TEXTURE)
		{
			return "texture";
		}

		return "Undefined";;
	}

}
