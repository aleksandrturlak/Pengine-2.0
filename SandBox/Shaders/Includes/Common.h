#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_debug_printf : enable
#extension GL_ARB_gpu_shader_int64 : enable

// Render pass constants - must match Core.h
#define MAX_PIPELINE_COUNT_PER_MATERIAL 8
#define MAX_PIPELINE_COUNT 128
#define MAX_INDIRECT_DRAW_COMMANDS 100000
#define MAX_BINDLESS_ENTITIES 20000
#define MAX_BINDLESS_TEXTURES 10000
#define MAX_CASCADE_COUNT 10

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

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
layout(buffer_reference, scalar) buffer BoneBuffer
{
	mat4 boneMatrices[MAX_BONES];
};

#define MAX_LODS 6
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

#define ENTITY_VALID 1 << 0
#define ENTITY_SKINNED 1 << 1

// Render pass indices
#define GBUFFER_PASS 0
#define CSM_PASS 1
#define POINT_SHADOW_PASS 2
#define SPOT_SHADOW_PASS 3
#define TRANSPARENT_PASS 4

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

struct EntityInfo
{
	mat4 transform;
	AABB aabb;
	MaterialInfoBuffer materialInfoBuffer;
	MeshInfoBuffer meshInfoBuffer;
	BoneBuffer boneBuffer;
	uint flags; // valid, skinned, etc.
};
