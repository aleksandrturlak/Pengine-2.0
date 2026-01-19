#version 450

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec2 uvA;
layout(location = 2) in vec3 normalA;
layout(location = 3) in vec4 tangentA;
layout(location = 4) in uint colorA;
layout(location = 5) in int materialIndexA;
layout(location = 6) in mat4 transformA;
layout(location = 10) in mat3 inverseTransformA;

layout(location = 0) out vec3 normalViewSpace;
layout(location = 1) out vec3 tangentViewSpace;
layout(location = 2) out vec3 bitangentViewSpace;
layout(location = 3) out vec2 uv;
layout(location = 4) out vec4 color;
layout(location = 5) out vec3 positionTangentSpace;
layout(location = 6) out vec3 cameraPositionTangentSpace;
layout(location = 7) flat out int materialIndex;

#include "Shaders/Includes/Camera.h"
layout(set = 0, binding = 0) uniform GlobalBuffer
{
	Camera camera;
};

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[10000];

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 1, binding = 1) buffer readonly BindlessMaterials
{
	DefaultMaterial materials[1000];
};

void main()
{
	DefaultMaterial material = materials[materialIndexA];

	vec4 windParams = unpackUnorm4x8(colorA);
	float stiffness = windParams.r;
    float oscillation = windParams.g;

	float windWave = sin(camera.time * camera.wind.frequency + float(gl_VertexIndex) * oscillation);

	float windInfluence = (1.0f - stiffness) * camera.wind.strength;
	vec3 windDisplacement = camera.wind.direction * windWave * windInfluence;

	vec3 positionWorldSpace = vec3(transformA * vec4(windDisplacement + positionA, 1.0f));
	gl_Position = camera.viewProjectionMat4 * vec4(positionWorldSpace, 1.0f);

	vec3 normalWorldSpace = normalize(inverseTransformA * normalize(normalA));
	vec3 tangentWorldSpace = normalize(inverseTransformA * normalize(tangentA.xyz));
	vec3 bitangentWorldSpace = normalize(cross(normalWorldSpace, tangentWorldSpace) * tangentA.w);

	if (material.useParallaxOcclusion > 0)
	{
		vec3 T   = tangentWorldSpace;
    	vec3 B   = bitangentWorldSpace;
   		vec3 N   = normalWorldSpace;
    	mat3 TBN = transpose(mat3(T, B, N));

		cameraPositionTangentSpace = TBN * camera.positionWorldSpace;
    	positionTangentSpace = TBN * positionWorldSpace.xyz;
	}

	normalViewSpace = normalize(mat3(camera.viewMat4) * normalWorldSpace);
	tangentViewSpace = normalize(mat3(camera.viewMat4) * tangentWorldSpace);
	bitangentViewSpace = normalize(mat3(camera.viewMat4) * bitangentWorldSpace);

	uv = uvA * material.uvTransform.xy + material.uvTransform.zw;

	color = vec4(1.0f);

	materialIndex = materialIndexA;
}
