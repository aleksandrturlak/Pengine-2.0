#version 450 core

#include "Shaders/Includes/Common.h"

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 positionWorldSpace;
layout(location = 2) flat in vec3 lightPositionWorldSpace;
layout(location = 3) flat in float radius;
layout(location = 4) flat in uint64_t materialBuffer;

layout(set = 3, binding = 0) uniform sampler2D bindlessTextures[MAX_BINDLESS_TEXTURES];

#include "Shaders/Includes/DefaultMaterial.h"
layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DefaultMaterial material;
};

void main()
{
	DefaultMaterial material = MaterialBufferReference(materialBuffer).material;

	if (material.useAlphaCutoff > 0)
	{
		if (texture(bindlessTextures[material.albedoTexture], uv).a < material.alphaCutoff)
		{
			discard;
		}
	}

    float lightDistance = length(positionWorldSpace.xyz - lightPositionWorldSpace);
    lightDistance = lightDistance / radius;
    gl_FragDepth = lightDistance;
}
