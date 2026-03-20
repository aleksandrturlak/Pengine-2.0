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

#include "Shaders/Includes/DissolveMaterial.h"
layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DissolveMaterial material;
};

#include "Shaders/Includes/ParallaxOcclusionMapping.h"

// --- Value noise for dissolve pattern ---

float hash(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(
		mix(hash(i),                hash(i + vec2(1.0, 0.0)), u.x),
		mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x),
		u.y);
}

// Three octaves of noise for a more organic dissolve pattern
float dissolveNoise(vec2 p)
{
	return valueNoise(p)       * 0.60
	     + valueNoise(p * 2.5) * 0.30
	     + valueNoise(p * 6.0) * 0.10;
}

// -----------------------------------------

void main()
{
	DissolveMaterial material = MaterialBufferReference(materialBuffer).material;

	vec2 finalUV = uv;
	if (material.useParallaxOcclusion > 0)
	{
		vec3 viewDir = normalize(cameraPositionTangentSpace - positionTangentSpace);
		finalUV = ParallaxOcclusionMapping(
			bindlessTextures[material.heightTexture],
			finalUV,
			viewDir,
			material.minParallaxLayers,
			material.maxParallaxLayers,
			material.parallaxHeightScale);
	}

	// Dissolve: unique noise per enemy via dissolveNoiseSeed offset
	vec2  noiseUV  = finalUV * 4.0 + vec2(material.dissolveNoiseSeed);
	float noiseVal = dissolveNoise(noiseUV);

	if (noiseVal < material.dissolveProgress)
		discard;

	vec4 albedoColor = texture(bindlessTextures[material.albedoTexture], finalUV) * material.albedoColor * color;
	if (material.useAlphaCutoff > 0)
	{
		if (albedoColor.a < material.alphaCutoff)
			discard;
	}

	vec3  metallicRoughness = texture(bindlessTextures[material.metallicRoughnessTexture], finalUV).xyz;
	float metallic          = metallicRoughness.b;
	float roughness         = metallicRoughness.g;
	float ao                = texture(bindlessTextures[material.aoTexture], finalUV).r;

	outAlbedo  = albedoColor;
	outShading = vec4(
		metallic  * material.metallicFactor,
		roughness * material.roughnessFactor,
		ao        * material.aoFactor,
		1.0);

	// Base emissive
	vec4 baseEmissive = texture(bindlessTextures[material.emissiveTexture], finalUV)
	                  * material.emissiveColor
	                  * material.emissiveFactor;

	// Edge glow at the dissolve boundary
	float edge         = smoothstep(material.dissolveProgress,
	                                material.dissolveProgress + material.dissolveEdgeWidth,
	                                noiseVal);
	float dissolveActive = step(0.001, material.dissolveProgress);
	vec4  edgeGlow       = material.dissolveEdgeColor * (1.0 - edge) * dissolveActive;

	outEmissive = baseEmissive + edgeGlow;

	vec3 normal = gl_FrontFacing ? normalViewSpace : -normalViewSpace;
	normal = normalize(normal);
	if (material.useNormalMap > 0)
	{
		mat3 TBN = mat3(normalize(tangentViewSpace), normalize(bitangentViewSpace), normal);
		normal   = texture(bindlessTextures[material.normalTexture], finalUV).xyz;
		normal   = normal * 2.0 - 1.0;
		outNormal = OctEncode(normalize(TBN * normal));
	}
	else
	{
		outNormal = OctEncode(normal);
	}
}
