#pragma once

#include "../Core/Core.h"

namespace Pengine
{

	struct PENGINE_API VertexDefault
	{
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
		glm::vec4 tangent;
		uint32_t color;
	};

	struct PENGINE_API VertexDefaultSkinned
	{
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
		glm::vec4 tangent;
		uint32_t color;
		glm::vec4 weights;
		glm::ivec4 boneIds;
	};

	struct PENGINE_API VertexPosition
	{
		glm::vec3 position;
		glm::vec2 uv;
	};

	struct PENGINE_API VertexNormal
	{
		glm::vec3 normal;
		glm::vec4 tangent;
	};

	struct PENGINE_API VertexSkinned
	{
		glm::vec4 weights;
		glm::ivec4 boneIds;
	};

	struct PENGINE_API VertexColor
	{
		uint32_t color;
	};

	struct PENGINE_API VertexSkinnedOutput
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec4 tangent;
	};

	struct PENGINE_API VertexLayout
	{
		uint32_t size;
		std::string tag;
	};

}
