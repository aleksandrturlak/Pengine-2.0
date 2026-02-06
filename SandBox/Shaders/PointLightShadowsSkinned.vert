#version 450

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec2 uvA;
layout(location = 2) in vec4 weightsA;
layout(location = 3) in ivec4 boneIdsA;
layout(location = 4) in mat4 transformA;
layout(location = 8) in int lightIndexA;
layout(location = 9) in int faceIndexA;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 positionWorldSpace;
layout(location = 2) flat out vec3 lightPositionWorldSpace;
layout(location = 3) flat out float radius;

#include "Shaders/Includes/Camera.h"
#include "Shaders/Includes/DirectionalLight.h"
#include "Shaders/Includes/PointLight.h"
#include "Shaders/Includes/SpotLight.h"
#include "Shaders/Includes/CSM.h"
#include "Shaders/Includes/SSS.h"

layout(set = 2, binding = 0) uniform Lights
{
	PointLight pointLights[32];
	int pointLightsCount;

	SpotLight spotLights[32];
	int spotLightsCount;

	DirectionalLight directionalLight;
	int hasDirectionalLight;

	float brightnessThreshold;

	CSM csm;

	PointLightShadows pointLightShadows;
    SpotLightShadows spotLightShadows;
    
    SSS sss;
};

#include "Shaders/Includes/Bones.h"
layout(set = 3, binding = 0) uniform BoneMatrices
{
	mat4 boneMatrices[MAX_BONES];
};

void main()
{
    vec4 totalPositionWorldSpace = vec4(0.0f);
	for (int i = 0 ; i < MAX_BONE_INFLUENCE ; i++)
	{
		if(boneIdsA[i] == -1)
		{
			continue;
		}
		if(boneIdsA[i] >= MAX_BONES)
		{
			totalPositionWorldSpace = vec4(positionA, 1.0f);
			break;
		}
		vec4 localPositionWorldSpace = boneMatrices[boneIdsA[i]] * vec4(positionA,1.0f);
		totalPositionWorldSpace += localPositionWorldSpace * weightsA[i];
	}

    positionWorldSpace = transformA * vec4(totalPositionWorldSpace.xyz, 1.0f);
	gl_Position = pointLights[lightIndexA].pointLightFaceInfos[faceIndexA].viewProjectionMat4 * positionWorldSpace;
    lightPositionWorldSpace = pointLights[lightIndexA].positionWorldSpace;
    radius = pointLights[lightIndexA].radius;
	uv = uvA;
}
