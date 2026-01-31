#version 450

layout(location = 0) in vec2 uv;

layout(set = 1, binding = 1) uniform sampler2D albedoTexture;
layout(set = 1, binding = 2) uniform sampler2D normalTexture;
layout(set = 1, binding = 3) uniform sampler2D metallicRoughnessTexture;
layout(set = 1, binding = 4) uniform sampler2D aoTexture;
layout(set = 1, binding = 5) uniform sampler2D emissiveTexture;
layout(set = 1, binding = 6) uniform sampler2D heightTexture;

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 1, binding = 0) uniform GBufferMaterial
{
	DefaultMaterial material;
};

layout(set = 2, binding = 0) uniform sampler2D bindlessTextures[10000];

void main()
{
	if (material.useAlphaCutoff > 0)
	{
		if (texture(bindlessTextures[material.albedoTexture], uv).a < material.alphaCutoff)
		{
			discard;
		}
	}
}
