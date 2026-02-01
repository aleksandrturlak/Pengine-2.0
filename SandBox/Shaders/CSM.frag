#version 450 core

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : enable

layout(location = 0) in vec2 uv;
layout(location = 1) flat in uint instanceIndex;

#include "Shaders/Includes/Common.h"
#include "Shaders/Includes/DefaultMaterial.h"

struct CSMInstanceData
{
	uint entityIndex;
	int cascadeIndex;
};

layout(buffer_reference, scalar) buffer DefaultMaterialBuffer
{
	DefaultMaterial material;
};

layout(set = 0, binding = 0, scalar) buffer readonly BindlessEntities
{
	EntityInfo entities[MAX_BINDLESS_ENTITIES];
};

layout(set = 1, binding = 1, scalar) buffer readonly CSMInstanceDataBuffer
{
	CSMInstanceData instanceData[MAX_INDIRECT_DRAW_COMMANDS];
};

layout(set = 2, binding = 0) uniform sampler2D bindlessTextures[MAX_BINDLESS_TEXTURES];

void main()
{
	CSMInstanceData instData = instanceData[instanceIndex];
	uint entityIndex = instData.entityIndex;
	
	EntityInfo entityInfo = entities[entityIndex];
	
	uint64_t materialBufferAddress = entityInfo.materialInfoBuffer.materialBuffers[GBUFFER_PASS];
	if (materialBufferAddress != 0)
	{
		DefaultMaterialBuffer matBuffer = DefaultMaterialBuffer(materialBufferAddress);
		DefaultMaterial material = matBuffer.material;
		
		if (material.useAlphaCutoff > 0)
		{
			float alpha = texture(bindlessTextures[material.albedoTexture], uv).a;
			if (alpha < material.alphaCutoff)
			{
				discard;
			}
		}
	}
}
