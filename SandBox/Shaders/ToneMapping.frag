#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D deferredTexture;
layout(set = 0, binding = 1) uniform sampler2D bloomTexture;
layout(set = 0, binding = 2) uniform sampler2D shadingTexture;
layout(set = 0, binding = 3) uniform sampler2D rawSSRTexture;
layout(set = 0, binding = 4) uniform sampler2D blurredReflectionsTexture;

layout(set = 0, binding = 5) uniform ToneMappingBuffer
{
	int toneMapperIndex;
	float gamma;
	int isReflectionsEnabled;
	int reflectionsTextureMipLevelCount;
};

#include "Shaders/Includes/ACES.h"

void main()
{
	vec3 bloom = texture(bloomTexture, uv).xyz;
	vec3 deferred = texture(deferredTexture, uv).xyz;
	vec4 shading = texture(shadingTexture, uv);
	float metallic = shading.r;
	float roughness = shading.g;
	float alpha = shading.a;

	vec4 reflectionColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);

	if (isReflectionsEnabled == 1)
	{
		float SSRExist = ceil(texture(blurredReflectionsTexture, uv).a);
		reflectionColor = textureLod(blurredReflectionsTexture, uv, roughness * reflectionsTextureMipLevelCount * SSRExist);
	}

	deferred = mix(deferred, reflectionColor.xyz, reflectionColor.a * isReflectionsEnabled * alpha);

	vec3 toneMappedColor;
	if (toneMapperIndex == 0)
	{
		toneMappedColor = pow(deferred + bloom, vec3(1.0f / gamma));
	}
	else if (toneMapperIndex == 1)
	{
		toneMappedColor = pow(ACES(deferred + bloom), vec3(1.0f / gamma));
	}

	outColor = vec4(toneMappedColor, 1.0f);
}
