#version 450

layout(location = 0) in vec3 positionViewSpace;
layout(location = 1) in vec3 positionWorldSpace;
layout(location = 2) in vec3 normalViewSpace;
layout(location = 3) in vec3 tangentViewSpace;
layout(location = 4) in vec3 bitangentViewSpace;
layout(location = 5) in vec2 uv;
layout(location = 6) in vec4 color;
layout(location = 7) in vec3 positionTangentSpace;
layout(location = 8) in vec3 cameraPositionTangentSpace;
layout(location = 9) flat in int materialIndex;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outNormal;
layout(location = 2) out vec4 outShading;
layout(location = 3) out vec4 outEmissive;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

#include "Shaders/Includes/Constants.h"
layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[MAX_BINDLESS_TEXTURES];

#include "Shaders/Includes/Common.h"
#include "Shaders/Includes/DirectionalLight.h"
#include "Shaders/Includes/PointLight.h"
#include "Shaders/Includes/SpotLight.h"
#include "Shaders/Includes/CSM.h"
#include "Shaders/Includes/SSS.h"

layout(set = 2, binding = 0) uniform sampler2D deferredAlbedoTexture;
layout(set = 2, binding = 1) uniform sampler2D deferredNormalTexture;
layout(set = 2, binding = 2) uniform sampler2D deferredShadingTexture;
layout(set = 2, binding = 3) uniform sampler2D deferredDepthTexture;
layout(set = 2, binding = 4) uniform sampler2D deferredSsaoTexture;
layout(set = 2, binding = 5) uniform sampler2D deferredSssTexture;
layout(set = 2, binding = 6) uniform sampler2DArray deferredCSMTexture;
layout(set = 2, binding = 7) uniform sampler2D deferredPointLightShadowMapTexture;
layout(set = 2, binding = 8) uniform sampler2D deferredSpotLightShadowMapTexture;

layout(set = 3, binding = 0) uniform Lights
{
	PointLight pointLights[32];
	int pointLightsCount;

	SpotLight spotLights[32];
	int spotLightsCount;

	DirectionalLight directionalLight;
	int hasDirectionalLight;

	float brightnessThreshold;

	PointLightShadows pointLightShadows;
    SpotLightShadows spotLightShadows;
    
    SSS sss;
};

#include "Shaders/Includes/ParallaxOcclusionMapping.h"

void main()
{
	//DefaultMaterial material = materials[materialIndex];
//
	//vec2 finalUV = uv;
	//if (material.useParallaxOcclusion > 0)
	//{
	//	vec3 viewDirectionTangentSpace = normalize(cameraPositionTangentSpace - positionTangentSpace);
	//	finalUV = ParallaxOcclusionMapping(
	//		bindlessTextures[material.heightTexture],
	//		finalUV,
	//		viewDirectionTangentSpace,
	//		material.minParallaxLayers,
	//		material.maxParallaxLayers,
	//		material.parallaxHeightScale);
	//}
//
	//vec4 albedoColor = texture(bindlessTextures[material.albedoTexture], finalUV) * material.albedoColor * color;
//
	//vec3 metallicRoughness = texture(bindlessTextures[material.metallicRoughnessTexture], finalUV).xyz;
	//float metallic = metallicRoughness.b;
	//float roughness = metallicRoughness.g;
	//float ao = texture(bindlessTextures[material.aoTexture], finalUV).r;
//
	//vec4 shading = vec4(
	//	metallic * material.metallicFactor,
	//	roughness * material.roughnessFactor,
	//	ao * material.aoFactor,
	//	albedoColor.a);
	//	
	//vec3 normalViewSpaceFinal = gl_FrontFacing ? normalViewSpace : -normalViewSpace;
	//normalViewSpaceFinal = normalize(normalViewSpaceFinal);
	//if (material.useNormalMap > 0)
	//{
	//	mat3 TBN = mat3(normalize(tangentViewSpace), normalize(bitangentViewSpace), normalViewSpaceFinal);
	//	normalViewSpaceFinal = texture(bindlessTextures[material.normalTexture], finalUV).xyz;
	//	normalViewSpaceFinal = normalViewSpaceFinal * 2.0f - 1.0f;
	//	normalViewSpaceFinal = normalize(TBN * normalViewSpaceFinal);
	//}
//
	//vec3 basicReflectivity = mix(vec3(0.05f), albedoColor.xyz, shading.x);
	//vec3 viewDirectionViewSpace = normalize(-positionViewSpace);
	//vec3 result = vec3(0.0f);
//
	//if (hasDirectionalLight == 1)
	//{
	//    vec3 shadow = vec3(0.0f);
	//	if (csm.isEnabled == 1)
	//	{
	//		shadow = CalculateCSM(
	//			deferredCSMTexture,
	//			csm,
	//			abs(positionViewSpace.z),
	//			positionWorldSpace,
	//			normalViewSpaceFinal,
	//			directionalLight.directionViewSpace);
	//	}
//
	//	result += CalculateDirectionalLight(
	//		directionalLight,
	//		viewDirectionViewSpace,
	//		basicReflectivity,
	//		normalViewSpaceFinal,
	//		albedoColor.xyz,
	//		shading.x,
	//		shading.y,
	//		shading.z,
	//		shadow,
	//		vec3(1.0f));
	//}
//
	//for (int i = 0; i < pointLightsCount; i++)
	//{
	//	PointLight pointLight = pointLights[i];
	//	vec3 toLightWorldSpace = pointLight.positionWorldSpace - positionWorldSpace;
	//	float distanceToPoint = length(toLightWorldSpace);
	//	if (distanceToPoint < pointLight.radius)
	//	{
	//		float shadow = 0.0f;
	//		if (pointLightShadows.isEnabled == 1 && pointLight.shadowMapIndex > -1)
	//		{
	//			shadow = CalculatePointLightShadow(
	//				deferredPointLightShadowMapTexture,
	//				pointLight,
	//				pointLightShadows,
	//				toLightWorldSpace,
	//				distanceToPoint);
	//		}
	//		
	//		result += CalculatePointLight(
	//			pointLight,
	//			viewDirectionViewSpace,
	//			positionViewSpace,
	//			basicReflectivity,
	//			normalViewSpaceFinal,
	//			albedoColor.xyz,
	//			shading.x,
	//			shading.y,
	//			shading.z,
	//			shadow);
	//	}
	//}
//
	//for (int i = 0; i < spotLightsCount; i++)
	//{
	//	SpotLight spotLight = spotLights[i];
	//	vec3 toLightWorldSpace = spotLight.positionWorldSpace - positionWorldSpace;
	//	float distanceToPoint = length(toLightWorldSpace);
	//	if (distanceToPoint < spotLight.radius)
	//	{
	//		float shadow = 0.0f;
	//		if (spotLightShadows.isEnabled == 1 && spotLight.shadowMapIndex > -1)
	//		{
	//			shadow = CalculateSpotLightShadow(
	//				deferredSpotLightShadowMapTexture,
	//				spotLight,
	//				spotLightShadows,
	//				positionWorldSpace,
	//				positionViewSpace,
	//				distanceToPoint);
	//		}
	//		
	//		result += CalculateSpotLight(
	//			spotLight,
	//			viewDirectionViewSpace,
	//			positionViewSpace,
	//			basicReflectivity,
	//			normalViewSpaceFinal,
	//			albedoColor.xyz,
	//			shading.x,
	//			shading.y,
	//			shading.z,
	//			shadow);
	//	}
	//}
//
	//vec3 emissiveColor = texture(bindlessTextures[material.emissiveTexture], finalUV).xyz;
    //outEmissive = max(vec4(emissiveColor * material.emissiveColor.xyz * material.emissiveFactor, albedoColor.a), vec4(IsBrightPixel(result, brightnessThreshold), albedoColor.a));
	//outColor = vec4(result, albedoColor.a);
	//outNormal = OctEncode(normalViewSpaceFinal);
	//outShading = shading;
}
