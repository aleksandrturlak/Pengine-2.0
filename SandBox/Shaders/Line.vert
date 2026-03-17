#version 450

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec3 colorA;

layout(location = 0) out vec3 color;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

void main()
{
	gl_Position = camera.viewProjectionMat4 * vec4(positionA, 1.0f);
	color = colorA;
}
