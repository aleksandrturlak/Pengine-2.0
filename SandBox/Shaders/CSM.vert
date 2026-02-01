#version 450

#extension GL_ARB_shader_viewport_layer_array : require

#include "Shaders/Includes/BindlessMeshes.h"
#include "Shaders/Includes/CSM.h"
#include "Shaders/Includes/DefaultMaterial.h"

struct CSMInstanceData
{
	uint entityIndex;
	int cascadeIndex;
};

layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DefaultMaterial material;
};

layout(set = 0, binding = 0, scalar) buffer readonly BindlessEntities
{
	EntityInfo entities[MAX_BINDLESS_ENTITIES];
};

layout(set = 1, binding = 0) uniform LightSpaceMatrices
{
	mat4 lightSpaceMatrices[MAX_CASCADE_COUNT];
	int cascadeCount;
};

layout(set = 1, binding = 1, scalar) buffer readonly CSMInstanceDataBuffer
{
	CSMInstanceData instanceData[MAX_INDIRECT_DRAW_COMMANDS];
};

layout(location = 0) out vec2 uv;
layout(location = 1) flat out uint outInstanceIndex;

void main()
{
	uint instanceIndex = gl_InstanceIndex;
	uint vertexIndex = gl_VertexIndex;

	CSMInstanceData instData = instanceData[instanceIndex];
	uint entityIndex = instData.entityIndex;
	int cascadeIndex = instData.cascadeIndex;

	EntityInfo entityInfo = entities[entityIndex];
	MeshInfoBuffer meshInfo = entityInfo.meshInfoBuffer;
	
	uint64_t materialBuffer = entityInfo.materialInfoBuffer.materialBuffers[GBUFFER_PASS];
	DefaultMaterial material = MaterialBufferReference(materialBuffer).material;
	
	uint index = meshInfo.meshBufferInfoBuffer.indexBuffer.indices[vertexIndex].index;
	VertexPosition vertex = meshInfo.meshBufferInfoBuffer.vertexBufferPosition.vertices[index];

	vec4 localPosition = vec4(vertex.position, 1.0f);

	// Apply skinning if entity is skinned
	if (bool(entityInfo.flags & ENTITY_SKINNED))
	{
		VertexSkinned skinned = meshInfo.meshBufferInfoBuffer.vertexBufferSkinned.skinned[index];
		vec4 totalPosition = vec4(0.0f);
		
		for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
		{
			int boneId = skinned.boneIds[i];
			if (boneId == -1)
			{
				continue;
			}
			if (boneId >= MAX_BONES)
			{
				totalPosition = localPosition;
				break;
			}
			vec4 bonePosition = entityInfo.boneBuffer.boneMatrices[boneId] * localPosition;
			totalPosition += bonePosition * skinned.weights[i];
		}
		
		localPosition = totalPosition;
	}

	vec4 worldPosition = entityInfo.transform * localPosition;
	gl_Position = lightSpaceMatrices[cascadeIndex] * worldPosition;
	gl_Layer = cascadeIndex;
	
	uv = vertex.uv * material.uvTransform.xy + material.uvTransform.zw;
	outInstanceIndex = instanceIndex;
}
