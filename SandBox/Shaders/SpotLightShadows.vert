#version 450

#include "Shaders/Includes/Common.h"

layout(location = 0) in int entityIndexA;
layout(location = 1) in int lightIndexA;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 positionWorldSpace;
layout(location = 2) flat out vec3 lightPositionWorldSpace;
layout(location = 3) flat out float radius;
layout(location = 4) flat out uint64_t materialBuffer;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

#include "Shaders/Includes/DirectionalLight.h"
#include "Shaders/Includes/PointLight.h"
#include "Shaders/Includes/SpotLight.h"
#include "Shaders/Includes/SSS.h"

layout(set = 1, binding = 0) uniform Lights
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

layout(set = 2, binding = 0, scalar) buffer readonly EntityBuffer
{
	EntityInfo entities[MAX_ENTITIES];
};

#include "Shaders/Includes/DefaultMaterial.h"
layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DefaultMaterial material;
};

void CalculateSkinning(
	in vec4 weights,
	in ivec4 boneIds,
	in BoneBuffer boneBuffer,
	inout vec3 position)
{
	vec4 totalPosition = vec4(0.0f);
	for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
	{
		if(boneIds[i] == -1) continue;
		if(boneIds[i] >= MAX_BONES)
		{
			totalPosition = vec4(position, 1.0f);
			break;
		}

		mat4 boneMat4 = boneBuffer.boneMatrices[boneIds[i]];
		vec4 localPosition = boneMat4 * vec4(position, 1.0f);
		totalPosition += localPosition * weights[i];
	}

	position = totalPosition.xyz;
}

void main()
{
	EntityInfo entityInfo = entities[entityIndexA];
	
	mat4 transform = entityInfo.transform;

	materialBuffer = entityInfo.materialInfoBuffer.materialBuffers[GBUFFER_PASS];
	DefaultMaterial material = MaterialBufferReference(materialBuffer).material;

	MeshInfoBuffer meshInfoBuffer = entityInfo.meshInfoBuffer;
	MeshBufferInfoBuffer meshBufferInfoBuffer = meshInfoBuffer.meshBufferInfoBuffer;

	uint index = meshBufferInfoBuffer.indexBuffer.indices[gl_VertexIndex].index;

	vec3 position = meshBufferInfoBuffer.vertexBufferPosition.vertices[index].position;
	
	if (bool(entityInfo.flags & ENTITY_SKINNED))
	{
		VertexSkinned vertexSkinned = meshBufferInfoBuffer.vertexBufferSkinned.skinned[index];
		CalculateSkinning(
			vertexSkinned.weights,
			vertexSkinned.boneIds,
			entityInfo.boneBuffer,
			position);
	}

	positionWorldSpace = transform * vec4(position, 1.0f);
	gl_Position = spotLights[lightIndexA].viewProjectionMat4 * positionWorldSpace;
    lightPositionWorldSpace = spotLights[lightIndexA].positionWorldSpace;
    radius = spotLights[lightIndexA].radius;
	uv = meshBufferInfoBuffer.vertexBufferPosition.vertices[index].uv * material.uvTransform.xy + material.uvTransform.zw;
}
