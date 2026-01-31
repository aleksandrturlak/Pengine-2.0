#version 450

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
	//DefaultMaterial material = materials[materialIndexA];
//
	//positionWorldSpace = (transformA * vec4(positionA, 1.0f)).xyz;
	//positionViewSpace = (camera.viewMat4 * vec4(positionWorldSpace, 1.0f)).xyz;
	//gl_Position = camera.projectionMat4 * vec4(positionViewSpace, 1.0f);
//
	//vec3 normalWorldSpace = normalize(inverseTransformA * normalize(normalA));
	//vec3 tangentWorldSpace = normalize(inverseTransformA * normalize(tangentA.xyz));
	//vec3 bitangentWorldSpace = normalize(cross(normalWorldSpace, tangentWorldSpace) * tangentA.w);
//
	//if (material.useParallaxOcclusion > 0)
	//{
	//	vec3 T   = tangentWorldSpace;
    //	vec3 B   = bitangentWorldSpace;
   	//	vec3 N   = normalWorldSpace;
    //	mat3 TBN = transpose(mat3(T, B, N));
//
	//	cameraPositionTangentSpace = TBN * camera.positionWorldSpace;
    //	positionTangentSpace = TBN * positionWorldSpace.xyz;
	//}
//
	//normalViewSpace = normalize(mat3(camera.viewMat4) * normalWorldSpace);
	//tangentViewSpace = normalize(mat3(camera.viewMat4) * tangentWorldSpace);
	//bitangentViewSpace = normalize(mat3(camera.viewMat4) * bitangentWorldSpace);
//
	//uv = uvA * material.uvTransform.xy + material.uvTransform.zw;
	//
	//color = unpackUnorm4x8(colorA);
//
	//materialIndex = materialIndexA;
}
