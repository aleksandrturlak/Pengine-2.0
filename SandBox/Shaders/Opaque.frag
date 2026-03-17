#version 460

#include "Shaders/Includes/Common.h"

layout(location = 0) in vec3 normalViewSpace;
layout(location = 1) in vec3 tangentViewSpace;
layout(location = 2) in vec3 bitangentViewSpace;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec4 color;
layout(location = 5) in vec3 positionTangentSpace;
layout(location = 6) in vec3 cameraPositionTangentSpace;
layout(location = 7) flat in uint64_t materialBuffer;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec2 outNormal;
layout(location = 2) out vec4 outShading;
layout(location = 3) out vec4 outEmissive;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[MAX_BINDLESS_TEXTURES];

#include "Shaders/Includes/DefaultMaterial.h"
layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DefaultMaterial material;
};

#include "Shaders/Includes/ParallaxOcclusionMapping.h"

void main()
{
	DefaultMaterial material = MaterialBufferReference(materialBuffer).material;

	vec2 finalUV = uv;
	if (material.useParallaxOcclusion > 0)
	{
		vec3 viewDirectionTangentSpace = normalize(cameraPositionTangentSpace - positionTangentSpace);
		finalUV = ParallaxOcclusionMapping(
			bindlessTextures[material.heightTexture],
			finalUV,
			viewDirectionTangentSpace,
			material.minParallaxLayers,
			material.maxParallaxLayers,
			material.parallaxHeightScale);
	}

	vec4 albedoColor = texture(bindlessTextures[material.albedoTexture], finalUV) * material.albedoColor * color;
	if (material.useAlphaCutoff > 0)
	{
		if (albedoColor.a < material.alphaCutoff)
		{
			discard;
		}
	}

	vec3 metallicRoughness = texture(bindlessTextures[material.metallicRoughnessTexture], finalUV).xyz;
	float metallic = metallicRoughness.b;
	float roughness = metallicRoughness.g;
	float ao = texture(bindlessTextures[material.aoTexture], finalUV).r;

	outAlbedo = albedoColor;
	outShading = vec4(
		metallic * material.metallicFactor,
		roughness * material.roughnessFactor,
		ao * material.aoFactor,
		1.0f);
	outEmissive = texture(bindlessTextures[material.emissiveTexture], finalUV) * material.emissiveColor * material.emissiveFactor;

	vec3 normal = gl_FrontFacing ? normalViewSpace : -normalViewSpace;
	normal = normalize(normal);
	if (material.useNormalMap > 0)
	{
		mat3 TBN = mat3(normalize(tangentViewSpace), normalize(bitangentViewSpace), normal);
		normal = texture(bindlessTextures[material.normalTexture], finalUV).xyz;
		normal = normal * 2.0f - 1.0f;
		outNormal = OctEncode(normalize(TBN * normal));
	}
	else
	{
		outNormal = OctEncode(normal);
	}
}
