#ifndef SSS_H
#define SSS_H

struct SSS
{
    int maxSteps;
    float maxRayDistance;
    float maxDistance;
    float minThickness;
    float maxThickness;
    vec2 viewportScale;
};

vec3 CalculateScreenSpaceShadows(
    sampler2D depthTexture,
    in vec3 positionViewSpace,
    in vec3 lightDirectionViewSpace,
    in vec2 viewRay,
    in mat4 projectionMat4,
    in SSS sss)
{
    if (-positionViewSpace.z > sss.maxDistance)
    {
        return vec3(0.0f);
    }

    vec3 rayPosition = positionViewSpace;
    vec3 rayDirection = lightDirectionViewSpace;

    float stepLength = sss.maxRayDistance / sss.maxSteps;
    vec3 rayStep = rayDirection * stepLength;

    float occlusion = 0.0f;
    vec2 rayUV = vec2(0.0f);
    for (uint i = 0; i < sss.maxSteps; i++)
    {
        rayPosition += rayStep;

		vec4 clipPos = projectionMat4 * vec4(rayPosition, 1.0f);
        if (clipPos.w <= 0.0f) break;
		vec3 ndc = clipPos.xyz / clipPos.w;
        rayUV = ndc.xy * 0.5f + 0.5f;
		
        if ((rayUV.x >= 0.0f && rayUV.x <= 1.0f) && (rayUV.y >= 0.0f && rayUV.y <= 1.0f))
        {
			rayUV.y = 1.0f - rayUV.y;
            float depthZ = CalculatePositionFromDepth(
				texture(depthTexture, rayUV).x,
				projectionMat4,
				viewRay).z;
            float depthDelta = rayPosition.z - depthZ;

            if (depthDelta > sss.minThickness && depthDelta < sss.maxThickness)
            {
                occlusion = 1.0f;
                break;
            }
        }
    }

    return vec3(occlusion);
}

#endif
