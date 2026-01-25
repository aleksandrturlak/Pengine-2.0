#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

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

struct MeshBufferInfo
{
	VertexBufferPosition vertexBufferPosition;
	VertexBufferNormal vertexBufferNormal;
	VertexBufferColor vertexBufferColor;
	VertexBufferSkinned vertexBufferSkinned;
	VertexBufferIndex indexBuffer;
};

layout(buffer_reference, scalar) buffer MeshBufferInfoBuffer
{
	MeshBufferInfo meshBufferInfo;
};

struct MeshInfo
{
	MeshBufferInfoBuffer meshBufferInfoBuffer;
	uint lodCount;
	uint flags;
	LodInfo lods[MAX_LODS];
};

layout(buffer_reference, scalar) buffer MeshInfoBuffer
{
	MeshInfo meshInfo;
};

struct AABB
{
	vec3 min;
	vec3 max;
};

#define ENTITY_VALID 1 << 0
#define ENTITY_SKINNED 1 << 1

struct EntityInfo
{
	mat4 transform;
	AABB aabb;
	int materialIndex;
	int pipelineId;
	MeshInfoBuffer meshInfoBuffer;
	BoneBuffer boneBuffer;
	uint flags; // valid, skinned, etc.
};
