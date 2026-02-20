#version 450

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec2 uvA;
layout(location = 2) in mat4 transformA;
layout(location = 6) in int lightIndexA;
layout(location = 7) in int faceIndexA;

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

void main()
{
    positionWorldSpace = transformA * vec4(positionA, 1.0f);
	gl_Position = pointLights[lightIndexA].pointLightFaceInfos[faceIndexA].viewProjectionMat4 * positionWorldSpace;
    lightPositionWorldSpace = pointLights[lightIndexA].positionWorldSpace;
    radius = pointLights[lightIndexA].radius;
	uv = uvA;
}
