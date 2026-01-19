#version 450

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec2 uvA;
layout(location = 2) in uint colorA;
layout(location = 3) in mat4 transformA;
layout(location = 7) in uint layersA;
layout(location = 8) in int materialIndexA;

layout(location = 0) out vec2 uv;
layout(location = 1) flat out uint layers;
layout(location = 2) flat out int materialIndex;

#include "Shaders/Includes/Camera.h"
layout(set = 2, binding = 0) uniform GlobalBuffer
{
	Camera camera;
};

void main()
{
	vec4 windParams = unpackUnorm4x8(colorA);
	float stiffness = windParams.r;
    float oscillation = windParams.g;

	float windWave = sin(camera.time * camera.wind.frequency + float(gl_VertexIndex) * oscillation);

	float windInfluence = (1.0f - stiffness) * camera.wind.strength;
	vec3 windDisplacement = camera.wind.direction * windWave * windInfluence;

	gl_Position = transformA * vec4(windDisplacement + positionA, 1.0f);
	materialIndex = materialIndexA;
	layers = layersA;
	uv = uvA;
}
