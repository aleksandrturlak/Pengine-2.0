// This shader is only for reflection, not for use!

#version 450

layout(location = 0) in vec3 positionA;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[10000];

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 2, binding = 0) buffer readonly Material
{
	DefaultMaterial material;
};

#include "Shaders/Includes/Common.h"
layout(set = 3, binding = 0, scalar) buffer readonly BindlessEntities
{
	EntityInfo entities[20000];
};

void main()
{
	gl_Position = vec4(positionA, 1.0f);
}
