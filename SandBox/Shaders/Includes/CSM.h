#ifndef CSM_H
#define CSM_H

#define MAX_CASCADE_COUNT 10

#ifndef POISSON_DISK_DEFINED
#define POISSON_DISK_DEFINED
const vec2 poissonDisk[16] = vec2[](
   vec2(-0.94201624, -0.39906216),
   vec2(0.94558609, -0.76890725),
   vec2(-0.094184101, -0.92938870),
   vec2(0.34495938, 0.29387760),
   vec2(-0.91588581, 0.45771432),
   vec2(-0.81544232, -0.87912464),
   vec2(-0.38277543, 0.27676845),
   vec2(0.97484398, 0.75648379),
   vec2(0.44323325, -0.97511554),
   vec2(0.53742981, -0.47373420),
   vec2(-0.26496911, -0.41893023),
   vec2(0.79197514, 0.19090188),
   vec2(-0.24188840, 0.99706507),
   vec2(-0.81409955, 0.91437590),
   vec2(0.19984126, 0.78641367),
   vec2(0.14383161, -0.14100790)
);
#endif

struct CSM
{
	int cascadeCount;
    float fogFactor;
    float maxDistance;
    int isEnabled;
    int filtering;
    int pcfRange;
    int visualize;
	mat4 lightSpaceMatrices[MAX_CASCADE_COUNT];
	float distances[MAX_CASCADE_COUNT];
    float biases[MAX_CASCADE_COUNT];
};

vec3 CalculateCSM(
	sampler2DArray CSMTexture,
	in CSM csm,
	in float depth,
	in vec3 positionWorldSpace,
	in vec3 normalViewSpace,
	in vec3 lightDirectionViewSpace)
{
    if (csm.cascadeCount == 0)
    {
        return vec3(0.0f);
    }

    int layer = -1;
    for (int i = 0; i < csm.cascadeCount; ++i)
    {
        if(depth < csm.distances[i])
        {
            layer = i;
            break;
        }
    }

    if (layer == -1)
    {
        layer = csm.cascadeCount;
    }

    vec4 uv = csm.lightSpaceMatrices[layer] * vec4(positionWorldSpace, 1.0f);
    uv.xyz /= uv.w;
    uv.xy = uv.xy * 0.5f + 0.5f;

    // Note: Only for Vulkan need to flip Y!
    uv.y = 1.0f - uv.y;

    float currentDepth = uv.z;

    if (currentDepth < -1.0f || currentDepth > 1.0f)
    {
        return vec3(0.0f);
    }

    float bias = max(csm.biases[layer] * (1.0f - dot(normalViewSpace, lightDirectionViewSpace)), 0.001f);
    if (layer == csm.cascadeCount)
    {
        bias *= 1.0f / (csm.maxDistance * 0.5f);
    }
    else
    {
        bias *= 1.0f / (csm.distances[layer] * 0.5f);
    }

    vec2 texelSize = 1.0f / vec2(textureSize(CSMTexture, 0));
   
    float shadow = 0.0f;
    if (csm.filtering == 1)
    {
        for (int x = -csm.pcfRange; x <= csm.pcfRange; ++x)
        {
            for (int y = -csm.pcfRange; y <= csm.pcfRange; ++y)
            {
                float pcfDepth = texture(CSMTexture, vec3(uv.xy + vec2(x, y) * texelSize, layer)).r;
                shadow += (currentDepth - bias) > pcfDepth ? 1.0f : 0.0f;
            }
        }
        shadow /= (csm.pcfRange * 2 + 1) * (csm.pcfRange * 2 + 1);
    }
    else if (csm.filtering == 2)
    {
        float random = fract(sin(dot(uv.xy * vec2(textureSize(CSMTexture, 0)), vec2(12.9898, 78.233))) * 43758.5453);
        float angle = random * 2.0 * 3.14159265;
        
        mat2 rotation = mat2(cos(angle), -sin(angle),
                            sin(angle), cos(angle));

        for (int i = 0; i < 16; i++)
        {
            vec2 offset = rotation * poissonDisk[i];
            float closestDepth = texture(CSMTexture, vec3(uv.xy + offset * texelSize, layer)).r;
            shadow += (currentDepth - bias) > closestDepth ? 1.0f : 0.0f;
        }
        shadow /= 16;
    }
    else
    {
        float shadowDepth = texture(CSMTexture, vec3(uv.xy, layer)).r;
        shadow += (currentDepth - bias) > shadowDepth ? 1.0f : 0.0f;
    }

    float fadeDistance = csm.maxDistance * (1.0f - csm.fogFactor);
    if (depth > fadeDistance)
    {
        float fogFactor = clamp((depth - fadeDistance) / (csm.maxDistance * csm.fogFactor), 0.0f, 1.0f);
        shadow *= (1.0f - fogFactor);
    }

    if (csm.visualize == 1)
    {
        vec3 visualized = vec3(0.0f);
        visualized[layer % 3] = shadow;
        return visualized;
    }

    return vec3(shadow);
}

#endif
