#version 450

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec2 uvA;
layout(location = 2) in mat4 transformA;

layout(location = 0) out vec2 uv;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 1, binding = 0) uniform GBufferMaterial
{
	DefaultMaterial material;
};

void main()
{
	gl_Position = camera.viewProjectionMat4 * transformA * vec4(positionA, 1.0f);
	uv = uvA * material.uvTransform.xy + material.uvTransform.zw;
}
