#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 positionWorldSpace;
layout(location = 2) flat in vec3 lightPositionWorldSpace;
layout(location = 3) flat in float radius;

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 0, binding = 0) uniform GBufferMaterial
{
	DefaultMaterial material;
};

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[10000];

void main()
{
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
