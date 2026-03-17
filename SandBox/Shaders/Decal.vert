#version 450

#include "Shaders/Includes/Common.h"

layout(location = 0) in vec3 positionA;
layout(location = 1) in uint64_t materialBufferA;
layout(location = 2) in mat4 transformA;
layout(location = 6) in mat4 inverseTransformA;

layout(location = 0) flat out uint64_t materialBuffer;
layout(location = 1) flat out mat4 inverseTransform;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

void main()
{
    inverseTransform = inverseTransformA;
    materialBuffer = MaterialInfoBuffer(materialBufferA).materialBuffers[DECALS_PASS];
    gl_Position = camera.viewProjectionMat4 * transformA * vec4(positionA, 1.0f);
}
