#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 1) flat in int materialIndex;

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[10000];

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 1, binding = 1) buffer readonly BindlessMaterials
{
	DefaultMaterial materials[1000];
};

void main()
{
	DefaultMaterial material = materials[materialIndex];
	if (material.useAlphaCutoff > 0)
	{
		if (texture(bindlessTextures[material.albedoTexture], uv).a < material.alphaCutoff)
		{
			discard;
		}
	}
}
