bool ProjectAABBToScreen(
    in vec3 aabbMin,
    in vec3 aabbMax,
    in mat4 viewProj,
    in vec2 viewportSize,
    out vec2 screenMin,
    out vec2 screenMax,
    out float aabbClosestDepth)
{
    const vec3 corners[8] = vec3[](
        vec3(aabbMin.x, aabbMin.y, aabbMin.z),
        vec3(aabbMax.x, aabbMin.y, aabbMin.z),
        vec3(aabbMin.x, aabbMax.y, aabbMin.z),
        vec3(aabbMax.x, aabbMax.y, aabbMin.z),
        vec3(aabbMin.x, aabbMin.y, aabbMax.z),
        vec3(aabbMax.x, aabbMin.y, aabbMax.z),
        vec3(aabbMin.x, aabbMax.y, aabbMax.z),
        vec3(aabbMax.x, aabbMax.y, aabbMax.z)
    );

    screenMin = vec2(1e9);
    screenMax = vec2(-1e9);
    aabbClosestDepth = 0.0;
    bool anyFront = false;
    bool anyBehind = false;

    for (int i = 0; i < 8; ++i)
    {
        vec4 clipPos = viewProj * vec4(corners[i], 1.0);

        if (clipPos.w <= 0.0)
        {
            anyBehind = true;
            continue;
        }

        vec3 ndc = clipPos.xyz / clipPos.w;
        vec2 screenPos = (ndc.xy * 0.5 + 0.5) * viewportSize;

        screenMin = min(screenMin, screenPos);
        screenMax = max(screenMax, screenPos);

        float depth = ndc.z;
        aabbClosestDepth = max(aabbClosestDepth, depth);

        anyFront = true;
    }

    if (!anyFront) return false;

    if (anyBehind)
    {
        screenMin = vec2(0.0);
        screenMax = viewportSize - vec2(1.0);
        aabbClosestDepth = 1.0;
    }

    screenMin = clamp(screenMin, vec2(0.0), viewportSize - vec2(1.0));
    screenMax = clamp(screenMax, vec2(0.0), viewportSize - vec2(1.0));
    
    return true;
}

uint GetHiZMipLevel(
    in vec2 screenMin,
    in vec2 screenMax,
    in vec2 viewportSize,
    in uint maxMipLevel)
{
    vec2 size = screenMax - screenMin;
    float maxSize = max(size.x, size.y);
    
    float mip = log2(maxSize);
    
    uint level = uint(ceil(mip));
    level = min(level, maxMipLevel);
    
    return level;
}

bool IsAABBOccludedByHiZ(
    in vec3 aabbMin,
    in vec3 aabbMax,
    in mat4 viewProjection,
    in vec2 viewportSize,
    in uint maxMipLevel,
    in float depthBias)
{
    vec2 screenMin, screenMax;
    float aabbClosestDepth;
    
    if (!ProjectAABBToScreen(aabbMin, aabbMax, viewProjection, viewportSize,
                            screenMin, screenMax, aabbClosestDepth))
    {
        return true;
    }
    
    if (screenMax.x - screenMin.x < 1.0 || screenMax.y - screenMin.y < 1.0)
    {
        return false;
    }
    
    // Clamp to 1: mip 0 slot is not written by HiZReduce (full-res copy removed).
    uint mipLevel = max(1u, GetHiZMipLevel(screenMin, screenMax, viewportSize, maxMipLevel));

    // Compute texel rect and coarsen mip until it fits within a 4x4 sample budget.
    // This ensures we always sample the entire AABB footprint, not just a corner.
    vec2 mipSize;
    ivec2 texelMin, texelMax;
    for (;;)
    {
        float scale = exp2(float(mipLevel));
        mipSize = viewportSize / scale;
        texelMin = ivec2(screenMin / scale);
        texelMax = ivec2(screenMax / scale);

        int flippedMinY = int(mipSize.y) - 1 - texelMax.y;
        int flippedMaxY = int(mipSize.y) - 1 - texelMin.y;
        texelMin.y = flippedMinY;
        texelMax.y = flippedMaxY;

        texelMin = clamp(texelMin, ivec2(0), ivec2(mipSize) - ivec2(1));
        texelMax = clamp(texelMax, ivec2(0), ivec2(mipSize) - ivec2(1));

        // If rect fits within 4x4, stop. Otherwise use a coarser mip.
        if (all(lessThanEqual(texelMax - texelMin, ivec2(3))) || mipLevel >= maxMipLevel)
            break;

        mipLevel++;
    }

    float farthestHiZDepth = 1.0;

    for (int y = texelMin.y; y <= texelMax.y; y++)
    {
        for (int x = texelMin.x; x <= texelMax.x; x++)
        {
            float d = imageLoad(hiZPyramid[mipLevel], ivec2(x, y)).r;
            farthestHiZDepth = min(farthestHiZDepth, d);
        }
    }
    
    if (aabbClosestDepth < farthestHiZDepth - depthBias)
    {
        return true;
    }
    
    return false;
}
