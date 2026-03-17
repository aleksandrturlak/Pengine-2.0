#ifndef CONSTANTS_H
#define CONSTANTS_H

// Render pass constants - must match Core.h
#define MAX_PIPELINE_COUNT_PER_MATERIAL 8
#define MAX_PIPELINE_COUNT 128
#define MAX_INDIRECT_DRAW_COMMANDS 100000
#define MAX_ENTITIES 20000
#define MAX_BINDLESS_TEXTURES 10000
#define MAX_CASCADE_COUNT 10

#define MAX_BONES 100
#define MAX_BONE_INFLUENCE 4
#define MAX_LODS 6

#define ENTITY_VALID 1 << 0
#define ENTITY_SKINNED 1 << 1

// Render pass indices
#define GBUFFER_PASS 0
#define DECALS_PASS 1
#define CSM_PASS 2
#define POINT_SHADOW_PASS 3
#define SPOT_SHADOW_PASS 4
#define TRANSPARENT_PASS 5

#endif
