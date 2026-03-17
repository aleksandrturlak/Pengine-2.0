#version 450

#extension GL_ARB_shader_viewport_layer_array : require

#include "Shaders/Includes/Common.h"

#include "Shaders/Includes/DefaultMaterial.h"
layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DefaultMaterial material;
};

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

layout(set = 1, binding = 0, scalar) buffer readonly EntityBuffer
{
	EntityInfo entities[MAX_ENTITIES];
};

layout(set = 2, binding = 0, scalar) buffer readonly IndirectDrawCommands
{
	DrawIndirectCommand drawCommands[MAX_INDIRECT_DRAW_COMMANDS];
};

layout(set = 2, binding = 1) buffer readonly IndirectDrawCommandCount
{
	uint count[MAX_CASCADE_COUNT * MAX_PIPELINE_COUNT];
};

layout(set = 2, binding = 2) buffer readonly PipelineInfoBuffer
{
	uint pipelineCommandOffset[MAX_CASCADE_COUNT * MAX_PIPELINE_COUNT];
};

layout(set = 2, binding = 3, scalar) buffer readonly CSMInstanceDataBuffer
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

	if (bool(entityInfo.flags & ENTITY_SKINNED))
	{
		localPosition = vec4(entityInfo.skinnedVertexBuffer.skinnedVertices[index].position, 1.0f);
	}

	vec4 worldPosition = vec4(QuatRotate(entityInfo.rotation, localPosition.xyz * entityInfo.scale) + entityInfo.position, 1.0);
	gl_Position = csm.lightSpaceMatrices[cascadeIndex] * worldPosition;
	gl_Layer = cascadeIndex;
	
	uv = vertex.uv * material.uvTransform.xy + material.uvTransform.zw;
	outInstanceIndex = instanceIndex;
}
