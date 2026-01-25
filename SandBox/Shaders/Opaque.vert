#version 450

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

#include "Shaders/Includes/BindlessMeshes.h"
layout(set = 2, binding = 0, scalar) buffer readonly BindlessEntities
{
	EntityInfo entities[20000];
};

layout(set = 3, binding = 0, scalar) uniform MaterialBuffer
{
	DefaultMaterial materialTest;
};

void CalculateSkinning(
	in vec4 weights,
	in ivec4 boneIds,
	in BoneBuffer boneBuffer,
	inout vec3 position,
	inout vec3 normal,
	inout vec3 tangent,
	inout vec3 bitangent)
{
	vec4 totalPosition = vec4(0.0f);
	vec3 totalNormal = vec3(0.0f);
	vec3 totalTangent = vec3(0.0f);
	vec3 totalBitangent = vec3(0.0f);
	for(int i = 0; i < MAX_BONE_INFLUENCE; i++)
	{
		if(boneIds[i] == -1) continue;
		if(boneIds[i] >= MAX_BONES)
		{
			totalPosition = vec4(position, 1.0f);
			totalNormal = normal;
			totalTangent = tangent;
			totalBitangent = bitangent;
			break;
		}

		mat4 boneMat4 = boneBuffer.boneMatrices[boneIds[i]];
		vec4 localPosition = boneMat4 * vec4(position, 1.0f);
		totalPosition += localPosition * weights[i];

		mat3 boneMat3 = mat3(boneMat4);

		vec3 localNormal = boneMat3 * normal;
		totalNormal += localNormal * weights[i];

		vec3 localTangent = boneMat3 * tangent;
		totalTangent += localTangent * weights[i];

		vec3 localBitangent = boneMat3 * bitangent;
		totalBitangent += localBitangent * weights[i];
	}

	position = totalPosition.xyz;
	normal = totalNormal;
	tangent = totalTangent;
	bitangent = totalBitangent;
}

void main()
{
	EntityInfo entityInfo = entities[gl_InstanceIndex];
	materialIndex = entityInfo.materialIndex;
	
	mat4 transform = entityInfo.transform;

	DefaultMaterial material = materials[materialIndex];
	MeshInfo meshInfo = entityInfo.meshInfoBuffer.meshInfo;
	MeshBufferInfo meshBufferInfo = meshInfo.meshBufferInfoBuffer.meshBufferInfo;

	uint index = meshBufferInfo.indexBuffer.indices[gl_VertexIndex].index;

	vec3 position = meshBufferInfo.vertexBufferPosition.vertices[index].position;
	vec3 normal = meshBufferInfo.vertexBufferNormal.normals[index].normal;
	vec4 fullTangent = meshBufferInfo.vertexBufferNormal.normals[index].tangent;
	vec3 tangent = fullTangent.xyz;
	vec3 bitangent = cross(normal, tangent) * fullTangent.w;

	if (bool(entityInfo.flags & ENTITY_SKINNED))
	{
		VertexSkinned vertexSkinned = meshBufferInfo.vertexBufferSkinned.skinned[index];
		CalculateSkinning(
			vertexSkinned.weights,
			vertexSkinned.boneIds,
			entityInfo.boneBuffer,
			position,
			normal,
			tangent,
			bitangent);
	}

	vec4 positionWorldSpace = transform * vec4(position, 1.0f);
	gl_Position = camera.viewProjectionMat4 * positionWorldSpace;

	mat3 inverseTransform = mat3(transpose(inverse(transform)));

	vec3 normalWorldSpace = normalize(inverseTransform * normalize(normal));
	vec3 tangentWorldSpace = normalize(inverseTransform * normalize(tangent));
	vec3 bitangentWorldSpace = normalize(inverseTransform * normalize(bitangent));

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

	uv = meshBufferInfo.vertexBufferPosition.vertices[index].uv * material.uvTransform.xy + material.uvTransform.zw;

	color = unpackUnorm4x8(meshBufferInfo.vertexBufferColor.colors[index].color);
}
