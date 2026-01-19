#version 450

layout(location = 0) in vec3 normalViewSpace;
layout(location = 1) in vec3 tangentViewSpace;
layout(location = 2) in vec3 bitangentViewSpace;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec4 color;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec2 outNormal;
layout(location = 2) out vec4 outShading;
layout(location = 3) out vec4 outEmissive;

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 1, binding = 0) uniform GBufferMaterial
{
	DefaultMaterial material;
	vec2 size;
	vec4 firstColor;
	vec4 secondColor;
};

#include "Shaders/Includes/Camera.h"
layout(set = 0, binding = 0) uniform GlobalBuffer
{
	Camera camera;
};

layout(set = 2, binding = 0) uniform sampler2D bindlessTextures[10000];

layout(set = 2, binding = 1) buffer readonly BindlessMaterials
{
	DefaultMaterial materials[1000];
};

void main()
{
	ivec2 coords = ivec2(size * uv) % 2;

	vec4 checkerBoardColor = secondColor;
	if (coords.x == coords.y)
	{
		checkerBoardColor = firstColor;
	}

	vec4 albedoColor = checkerBoardColor * texture(bindlessTextures[materials[0].albedoTexture], uv) * materials[0].albedoColor * color;
	if (materials[0].useAlphaCutoff > 0)
	{
		if (albedoColor.a < materials[0].alphaCutoff)
		{
			discard;
		}
	}

	vec3 metallicRoughness = texture(bindlessTextures[materials[0].metallicRoughnessTexture], uv).xyz;
	float metallic = metallicRoughness.b;
	float roughness = metallicRoughness.g;
	float ao = texture(bindlessTextures[materials[0].aoTexture], uv).r;

	outAlbedo = albedoColor;
	outShading = vec4(
		metallic * materials[0].metallicFactor,
		roughness * materials[0].roughnessFactor,
		ao * materials[0].aoFactor,
		1.0f);
	outEmissive = texture(bindlessTextures[materials[0].emissiveTexture], uv) * materials[0].emissiveColor * materials[0].emissiveFactor;

	vec3 normalViewSpaceFinal = gl_FrontFacing ? normalViewSpace : -normalViewSpace;
	normalViewSpaceFinal = normalize(normalViewSpaceFinal);
	if (materials[0].useNormalMap > 0)
	{
		mat3 TBN = mat3(normalize(tangentViewSpace), normalize(bitangentViewSpace), normalViewSpaceFinal);
		normalViewSpaceFinal = texture(bindlessTextures[materials[0].normalTexture], uv).xyz;
		normalViewSpaceFinal = normalViewSpaceFinal * 2.0f - 1.0f;
		outNormal = OctEncode(normalize(TBN * normalViewSpaceFinal));
	}
	else
	{
		outNormal = OctEncode(normalViewSpaceFinal);
	}
}
