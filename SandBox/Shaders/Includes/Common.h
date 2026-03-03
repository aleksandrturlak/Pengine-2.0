#ifndef COMMON_H
#define COMMON_H

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_debug_printf : enable
#extension GL_ARB_gpu_shader_int64 : enable

#include "Shaders/Includes/Constants.h"

struct VertexPosition
{
	vec3 position;
	vec2 uv;
};

struct VertexNormal
{
	vec3 normal;
	vec4 tangent;
};

struct VertexColor
{
	uint color;
};

struct VertexSkinned
{
	vec4 weights;
	ivec4 boneIds;
};

struct VertexIndex
{
	uint index;
};

layout(buffer_reference, scalar) buffer VertexBufferPosition
{
    VertexPosition vertices[];
};

layout(buffer_reference, scalar) buffer VertexBufferNormal
{
    VertexNormal normals[];
};

layout(buffer_reference, scalar) buffer VertexBufferColor
{
    VertexColor colors[];
};

layout(buffer_reference, scalar) buffer VertexBufferSkinned
{
    VertexSkinned skinned[];
};

layout(buffer_reference, scalar) buffer VertexBufferIndex
{
    VertexIndex indices[];
};

layout(buffer_reference, scalar) buffer BoneBuffer
{
	mat4 boneMatrices[MAX_BONES];
};

struct SkinnedVertex
{
	vec3 position;
	vec3 normal;
	vec4 tangent;
};

layout(buffer_reference, scalar) buffer SkinnedVertexBuffer
{
	SkinnedVertex skinnedVertices[];
};

struct LodInfo
{
	uint indexOffset;
	uint indexCount;
	float distanceThreshold;
};

layout(buffer_reference, scalar) buffer MeshBufferInfoBuffer
{
	VertexBufferPosition vertexBufferPosition;
	VertexBufferNormal vertexBufferNormal;
	VertexBufferColor vertexBufferColor;
	VertexBufferSkinned vertexBufferSkinned;
	VertexBufferIndex indexBuffer;
};

layout(buffer_reference, scalar) buffer MeshInfoBuffer
{
	MeshBufferInfoBuffer meshBufferInfoBuffer;
	uint lodCount;
	uint flags;
	LodInfo lods[MAX_LODS];
};

struct AABB
{
	vec3 min;
	vec3 max;
};

layout(buffer_reference, scalar) buffer BaseMaterialInfoBuffer
{
	uint pipelineIds[MAX_PIPELINE_COUNT_PER_MATERIAL];
};

layout(buffer_reference, scalar) buffer MaterialInfoBuffer
{
	uint64_t materialBuffers[MAX_PIPELINE_COUNT_PER_MATERIAL];
	BaseMaterialInfoBuffer baseMaterialInfoBuffer;
	uint64_t pipelineFlags;
};

vec3 QuatRotate(vec4 q, vec3 v)
{
	vec3 qv = q.xyz;
	vec3 t = 2.0 * cross(qv, v);
	return v + q.w * t + cross(qv, t);
}

struct EntityInfo
{
	vec4 rotation;
	vec3 position;
	vec3 scale;
	AABB aabb;
	MaterialInfoBuffer materialInfoBuffer;
	MeshInfoBuffer meshInfoBuffer;
	BoneBuffer boneBuffer;
	SkinnedVertexBuffer skinnedVertexBuffer;
	uint flags; // valid, skinned, etc.
};

struct DrawIndirectCommand
{
	uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint firstInstance;
};

struct CSMInstanceData
{
	uint entityIndex;
	int cascadeIndex;
};

vec3 IsBrightPixel(in vec3 color, in float threshold)
{
	const vec3 colorSRGB = pow(color, vec3(1.0f / 2.2f));
    return dot(colorSRGB, vec3(0.2126f, 0.7152f, 0.0722f)) > threshold ? colorSRGB : vec3(0.0f);
}

vec2 BarycentricLerp(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 BarycentricLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

#endif
