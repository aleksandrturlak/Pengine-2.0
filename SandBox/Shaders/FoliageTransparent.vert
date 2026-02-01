#version 460

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec2 uvA;
layout(location = 2) in vec3 normalA;
layout(location = 3) in vec4 tangentA;
layout(location = 4) in uint colorA;
layout(location = 5) in int materialIndexA;
layout(location = 6) in mat4 transformA;
layout(location = 10) in mat3 inverseTransformA;

layout(location = 0) out vec3 positionViewSpace;
layout(location = 1) out vec3 positionWorldSpace;
layout(location = 2) out vec3 normalViewSpace;
layout(location = 3) out vec3 tangentViewSpace;
layout(location = 4) out vec3 bitangentViewSpace;
layout(location = 5) out vec2 uv;
layout(location = 6) out vec4 color;
layout(location = 7) out vec3 positionTangentSpace;
layout(location = 8) out vec3 cameraPositionTangentSpace;
layout(location = 9) flat out int materialIndex;

#include "Shaders/Includes/Camera.h"
layout(set = 0, binding = 0) uniform GlobalBuffer
{
	Camera camera;
};

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[10000];

void main()
{
	// Wind animation for foliage
	vec4 windParams = unpackUnorm4x8(colorA);
	float stiffness = windParams.r;
	float oscillation = windParams.g;

	float windWave = sin(camera.time * camera.wind.frequency + float(gl_VertexIndex) * oscillation);
	float windInfluence = (1.0f - stiffness) * camera.wind.strength;
	vec3 windDisplacement = camera.wind.direction * windWave * windInfluence;

	// Apply wind displacement to position
	vec3 position = windDisplacement + positionA;

	positionWorldSpace = (transformA * vec4(position, 1.0f)).xyz;
	positionViewSpace = (camera.viewMat4 * vec4(positionWorldSpace, 1.0f)).xyz;
	gl_Position = camera.projectionMat4 * vec4(positionViewSpace, 1.0f);

	vec3 normalWorldSpace = normalize(inverseTransformA * normalize(normalA));
	vec3 tangentWorldSpace = normalize(inverseTransformA * normalize(tangentA.xyz));
	vec3 bitangentWorldSpace = normalize(cross(normalWorldSpace, tangentWorldSpace) * tangentA.w);

	normalViewSpace = normalize(mat3(camera.viewMat4) * normalWorldSpace);
	tangentViewSpace = normalize(mat3(camera.viewMat4) * tangentWorldSpace);
	bitangentViewSpace = normalize(mat3(camera.viewMat4) * bitangentWorldSpace);

	// Note: Parallax occlusion mapping would require material data, not implemented for transparent pass
	positionTangentSpace = vec3(0.0f);
	cameraPositionTangentSpace = vec3(0.0f);

	uv = uvA;
	color = unpackUnorm4x8(colorA);
	materialIndex = materialIndexA;
}

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
	
	mat4 transform = entityInfo.transform;

	materialIndex = int(gl_InstanceIndex);
	uint64_t materialBuffer = entityInfo.materialInfoBuffer.materialBuffers[GBUFFER_PASS];
	DefaultMaterial material = MaterialBufferReference(materialBuffer).material;

	MeshInfoBuffer meshInfoBuffer = entityInfo.meshInfoBuffer;
	MeshBufferInfoBuffer meshBufferInfoBuffer = meshInfoBuffer.meshBufferInfoBuffer;

	uint index = meshBufferInfoBuffer.indexBuffer.indices[gl_VertexIndex].index;

	vec3 position = meshBufferInfoBuffer.vertexBufferPosition.vertices[index].position;
	vec3 normal = meshBufferInfoBuffer.vertexBufferNormal.normals[index].normal;
	vec4 fullTangent = meshBufferInfoBuffer.vertexBufferNormal.normals[index].tangent;
	vec3 tangent = fullTangent.xyz;
	vec3 bitangent = cross(normal, tangent) * fullTangent.w;

	uint packedColor = meshBufferInfoBuffer.vertexBufferColor.colors[index].color;
	vec4 windParams = unpackUnorm4x8(packedColor);
	float stiffness = windParams.r;
	float oscillation = windParams.g;

	float windWave = sin(camera.time * camera.wind.frequency + float(gl_VertexIndex) * oscillation);
	float windInfluence = (1.0f - stiffness) * camera.wind.strength;
	vec3 windDisplacement = camera.wind.direction * windWave * windInfluence;

	position += windDisplacement;

	if (bool(entityInfo.flags & ENTITY_SKINNED))
	{
		VertexSkinned vertexSkinned = meshBufferInfoBuffer.vertexBufferSkinned.skinned[index];
		CalculateSkinning(
			vertexSkinned.weights,
			vertexSkinned.boneIds,
			entityInfo.boneBuffer,
			position,
			normal,
			tangent,
			bitangent);
	}

	positionWorldSpace = (transform * vec4(position, 1.0f)).xyz;
	positionViewSpace = (camera.viewMat4 * vec4(positionWorldSpace, 1.0f)).xyz;
	gl_Position = camera.projectionMat4 * vec4(positionViewSpace, 1.0f);

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

	uv = meshBufferInfoBuffer.vertexBufferPosition.vertices[index].uv * material.uvTransform.xy + material.uvTransform.zw;

	color = windParams;
}
