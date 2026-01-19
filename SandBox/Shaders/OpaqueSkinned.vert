#version 450

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec2 uvA;
layout(location = 2) in vec3 normalA;
layout(location = 3) in vec4 tangentA;
layout(location = 4) in uint colorA;
layout(location = 5) in vec4 weightsA;
layout(location = 6) in ivec4 boneIdsA;
layout(location = 7) in int materialIndexA;
layout(location = 8) in mat4 transformA;
layout(location = 12) in mat3 inverseTransformA;

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

#include "Shaders/Includes/Bones.h"
layout(set = 2, binding = 0) uniform BoneMatrices
{
	mat4 boneMatrices[MAX_BONES];
};

void main()
{
	DefaultMaterial material = materials[materialIndexA];

	vec3 normalWorldSpace = normalize(normalA);
	vec3 tangentWorldSpace = normalize(tangentA.xyz);
	vec3 bitangentWorldSpace = normalize(cross(normalWorldSpace, tangentWorldSpace) * tangentA.w);

	vec4 totalPositionWorldSpace = vec4(0.0f);
	vec3 totalNormalWorldSpace = vec3(0.0f);
	vec3 totalTangentWorldSpace = vec3(0.0f);
	vec3 totalBitangentWorldSpace = vec3(0.0f);
	for(int i = 0; i < MAX_BONE_INFLUENCE; i++)
	{
		if(boneIdsA[i] == -1)
			continue;
		if(boneIdsA[i] >= MAX_BONES)
		{
			totalPositionWorldSpace = vec4(positionA, 1.0f);
			totalNormalWorldSpace = normalWorldSpace;
			totalTangentWorldSpace = tangentWorldSpace;
			totalBitangentWorldSpace = bitangentWorldSpace;
			break;
		}
		vec4 localPositionWorldSpace = boneMatrices[boneIdsA[i]] * vec4(positionA, 1.0f);
		totalPositionWorldSpace += localPositionWorldSpace * weightsA[i];

		mat3 boneMat3 = mat3(boneMatrices[boneIdsA[i]]);

		vec3 localNormalWorldSpace = boneMat3 * normalWorldSpace;
		totalNormalWorldSpace += localNormalWorldSpace * weightsA[i];

		vec3 localTangentWorldSpace = boneMat3 * tangentWorldSpace;
		totalTangentWorldSpace += localTangentWorldSpace * weightsA[i];

		vec3 localBitangentWorldSpace = boneMat3 * bitangentWorldSpace;
		totalBitangentWorldSpace += localBitangentWorldSpace * weightsA[i];
	}

	vec4 positionWorldSpace = transformA * totalPositionWorldSpace;
	gl_Position = camera.viewProjectionMat4 * positionWorldSpace;

	totalNormalWorldSpace = normalize(inverseTransformA * totalNormalWorldSpace);
	totalTangentWorldSpace = normalize(inverseTransformA * totalTangentWorldSpace);
	totalBitangentWorldSpace = normalize(inverseTransformA * totalBitangentWorldSpace);

	if (material.useParallaxOcclusion > 0)
	{
		vec3 T   = normalize(totalTangentWorldSpace);
    	vec3 B   = normalize(totalBitangentWorldSpace);
   		vec3 N   = normalize(totalNormalWorldSpace);
    	mat3 TBN = transpose(mat3(T, B, N));

		cameraPositionTangentSpace = TBN * camera.positionWorldSpace;
    	positionTangentSpace = TBN * positionWorldSpace.xyz;
	}

	normalViewSpace = normalize(mat3(camera.viewMat4) * totalNormalWorldSpace);
	tangentViewSpace = normalize(mat3(camera.viewMat4) * totalTangentWorldSpace);
	bitangentViewSpace = normalize(mat3(camera.viewMat4) * totalBitangentWorldSpace);

	uv = uvA * material.uvTransform.xy + material.uvTransform.zw;

	color = unpackUnorm4x8(colorA);

	materialIndex = materialIndexA;
}
