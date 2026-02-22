#ifndef CAMERA_SET_H
#define CAMERA_SET_H

#include "Shaders/Includes/Camera.h"
#include "Shaders/Includes/CSM.h"

#extension GL_EXT_scalar_block_layout : require

#define CAMERA_SET(setIndex)                                                         \
layout(set = setIndex, binding = 0) uniform CameraBuffer                             \
{                                                                                    \
	Camera camera;                                                                   \
};                                                                                   \
                                                                                     \
layout(set = setIndex, binding = 1) uniform CSMBuffer                                \
{                                                                                    \
	CSM csm;                                                                         \
};                                                                                   \
                                                                                     \
layout(set = setIndex, binding = 2, scalar) uniform PointLightShadowMapIndicesBuffer \
{                                                                                    \
	int pointLightShadowMapIndices[32];                                              \
};                                                                                   \

#endif
