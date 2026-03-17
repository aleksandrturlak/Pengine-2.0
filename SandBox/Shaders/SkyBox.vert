#version 450

layout(location = 0) in vec3 positionA;

layout(location = 0) out vec3 uv;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

void main()
{
	gl_Position = camera.projectionMat4 * camera.inverseRotationMat4 * vec4(positionA, 1.0f);
	gl_Position.z = 0.0f;
	uv = positionA;
}
