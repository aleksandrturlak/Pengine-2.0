#version 460

#include "Shaders/Includes/Common.h"

layout(location = 0) out vec3 normalViewSpace;
layout(location = 1) out vec3 tangentViewSpace;
layout(location = 2) out vec3 bitangentViewSpace;
layout(location = 3) out vec2 uv;
layout(location = 4) out vec4 color;
layout(location = 5) out vec3 positionTangentSpace;
layout(location = 6) out vec3 cameraPositionTangentSpace;
layout(location = 7) flat out uint64_t materialBuffer;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[MAX_BINDLESS_TEXTURES];

layout(set = 2, binding = 0, scalar) buffer readonly EntityBuffer
{
	EntityInfo entities[MAX_ENTITIES];
};

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 3, binding = 0, scalar) uniform MaterialBuffer
{
	DefaultMaterial material;
};

layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DefaultMaterial material;
};

void main()
{
	EntityInfo entityInfo = entities[gl_InstanceIndex];
	
	materialBuffer = entityInfo.materialInfoBuffer.materialBuffers[GBUFFER_PASS];
	DefaultMaterial material = MaterialBufferReference(materialBuffer).material;

	MeshInfoBuffer meshInfoBuffer = entityInfo.meshInfoBuffer;
	MeshBufferInfoBuffer meshBufferInfoBuffer = meshInfoBuffer.meshBufferInfoBuffer;

	uint index = meshBufferInfoBuffer.indexBuffer.indices[gl_VertexIndex].index;

	vec3 position = meshBufferInfoBuffer.vertexBufferPosition.vertices[index].position;
	vec3 normal = meshBufferInfoBuffer.vertexBufferNormal.normals[index].normal;
	vec4 fullTangent = meshBufferInfoBuffer.vertexBufferNormal.normals[index].tangent;
	vec3 tangent = fullTangent.xyz;
	vec3 bitangent = cross(normal, tangent) * fullTangent.w;

	if (bool(entityInfo.flags & ENTITY_SKINNED))
	{
		SkinnedVertex sv = entityInfo.skinnedVertexBuffer.skinnedVertices[index];
		position  = sv.position;
		normal    = sv.normal;
		tangent   = sv.tangent.xyz;
		bitangent = cross(sv.normal, sv.tangent.xyz) * sv.tangent.w;
	}

	vec4 positionWorldSpace = vec4(QuatRotate(entityInfo.rotation, position * entityInfo.scale) + entityInfo.position, 1.0);
	gl_Position = camera.viewProjectionMat4 * positionWorldSpace;

	vec3 normalWorldSpace = normalize(QuatRotate(entityInfo.rotation, normalize(normal) / entityInfo.scale));
	vec3 tangentWorldSpace = normalize(QuatRotate(entityInfo.rotation, normalize(tangent) / entityInfo.scale));
	vec3 bitangentWorldSpace = normalize(QuatRotate(entityInfo.rotation, normalize(bitangent) / entityInfo.scale));

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

	uv = meshBufferInfoBuffer.vertexBufferPosition.vertices[index].uv * material.uvTransform.xy + material.uvTransform.zw;

	color = unpackUnorm4x8(meshBufferInfoBuffer.vertexBufferColor.colors[index].color);
}
