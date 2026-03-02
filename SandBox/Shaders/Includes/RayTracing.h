#ifndef RAY_TRACING_H
#define RAY_TRACING_H

#include "Shaders/Includes/DefaultMaterial.h"

#include "Shaders/Includes/DefaultMaterial.h"
layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DefaultMaterial material;
};

float TraceShadowRay(
    uint rayFlags,
    uint cullMask,
    vec3 origin,
    float tMin,
    vec3 direction,
    float tMax)
{
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(
        rayQuery,
        topLevelAS,
        rayFlags,
        cullMask,
        origin,
        tMin,
        direction,
        tMax);

    while(rayQueryProceedEXT(rayQuery))
    {
        if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
        {
            uint customId = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, false);

            vec2 barycentrics2D = rayQueryGetIntersectionBarycentricsEXT(rayQuery, false);
            vec3 barycentrics = vec3(1.0 - barycentrics2D.x - barycentrics2D.y, 
                barycentrics2D.x, 
                barycentrics2D.y);

            EntityInfo entityInfo = entities[customId];

            uint64_t materialBuffer = entityInfo.materialInfoBuffer.materialBuffers[GBUFFER_PASS];
            DefaultMaterial material = MaterialBufferReference(materialBuffer).material;

            MeshInfoBuffer meshInfoBuffer = entityInfo.meshInfoBuffer;
            MeshBufferInfoBuffer meshBufferInfoBuffer = meshInfoBuffer.meshBufferInfoBuffer;

            uint primitiveId = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, false);

            uint idx0 = meshBufferInfoBuffer.indexBuffer.indices[primitiveId * 3 + 0].index;
            uint idx1 = meshBufferInfoBuffer.indexBuffer.indices[primitiveId * 3 + 1].index;
            uint idx2 = meshBufferInfoBuffer.indexBuffer.indices[primitiveId * 3 + 2].index;

            vec2 uv0 = meshBufferInfoBuffer.vertexBufferPosition.vertices[idx0].uv;
            vec2 uv1 = meshBufferInfoBuffer.vertexBufferPosition.vertices[idx1].uv;
            vec2 uv2 = meshBufferInfoBuffer.vertexBufferPosition.vertices[idx2].uv;
            vec2 uv = BarycentricLerp(uv0, uv1, uv2, barycentrics) * material.uvTransform.xy + material.uvTransform.zw;

            vec4 albedoColor = texture(bindlessTextures[material.albedoTexture], uv) * material.albedoColor;
            
            if (material.useAlphaCutoff > 0)
            {
                if (texture(bindlessTextures[material.albedoTexture], uv).a < material.alphaCutoff)
                {
                    continue;
                }
            }

            rayQueryConfirmIntersectionEXT(rayQuery);
            return 1.0f;
        }
    }

    return 0.0f;
}

#endif
