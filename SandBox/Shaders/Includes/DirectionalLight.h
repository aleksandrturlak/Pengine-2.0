#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include "Shaders/Includes/CommonPBR.h"

struct DirectionalLight
{
	vec3 color;
	float intensity;
	vec3 directionViewSpace;
	float ambient;
	vec3 directionWorldSpace;
};

vec3 CalculateDirectionalLight(
	in DirectionalLight light,
	in vec3 viewDirectionViewSpace,
	in vec3 basicReflectivity,
	in vec3 normalViewSpace,
	in vec3 albedo,
	in float metallic,
	in float roughness,
	in float ao,
	in vec3 shadow,
	in vec3 ssao)
{
	vec3 H = normalize(viewDirectionViewSpace + light.directionViewSpace);

	vec3 radiance = light.color * light.intensity;
	vec3 ambient = light.ambient * radiance * ao;

	float NdotV = max(dot(normalViewSpace, viewDirectionViewSpace), 0.0000001f);
	float NdotL = max(dot(normalViewSpace, light.directionViewSpace), 0.0000001f);
	float HdotV = max(dot(H, viewDirectionViewSpace), 0.0f);
	float NdotH = max(dot(normalViewSpace, H), 0.0f);

	float D = DistributionGGX(NdotH, roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	vec3 F = FresnelSchlick(HdotV, basicReflectivity);

	vec3 specular = D * G * F;
	specular /= 4.0 * NdotV * NdotL;// + 0.0001;

	vec3 kS = F;
	vec3 kD = vec3(1.0f) - kS;

	kD *= 1.0f - metallic;

	return ssao * ambient * albedo + (vec3(1.0f) - shadow) * (kD * albedo / PI + specular) * radiance * NdotL;
}

#endif
