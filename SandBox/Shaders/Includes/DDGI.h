#ifndef DDGI_H
#define DDGI_H

// ---------------------------------------------------------------------------
// DDGI shared utilities
//
// Defines the probe grid data structure and functions for:
//  - Octahedral mapping (encode/decode directions to/from 2D atlas texels)
//  - Atlas UV computation (probe index -> atlas UV)
//  - Chebyshev visibility test
//  - Trilinear irradiance sampling from the persistent probe atlases
//
// Include AFTER the CAMERA_SET macro so that camera is in scope.
// ---------------------------------------------------------------------------

struct DDGIData
{
    vec3  gridOrigin;          // World-space position of probe (0,0,0)
    uint  probeCountX;         // Probes along X
    uint  probeCountY;         // Probes along Y
    uint  probeCountZ;         // Probes along Z
    float probeSpacing;        // World-space distance between adjacent probes
    uint  raysPerProbe;        // Number of rays cast per probe per frame
    float hysteresisIrradiance; // Temporal blend factor for irradiance (e.g. 0.97)
    float hysteresisVisibility; // Temporal blend factor for visibility  (e.g. 0.90)
    vec2  irradianceAtlasSize; // Full atlas size in texels
    vec2  visibilityAtlasSize; // Full atlas size in texels
    // Texels per probe in each atlas (excluding border)
    uint  irradianceProbeSize; // Default: 8
    uint  visibilityProbeSize; // Default: 16
    int   isEnabled;
    uint  frameIndex;  // 0 on first frame after init; blend shader uses this to skip hysteresis
};

// ---------------------------------------------------------------------------
// Octahedral helpers — note: the engine's Camera.h already defines OctEncode
// and OctDecode for normals using the [0,1] convention.  DDGI probes use the
// raw [-1,1] form internally, so we keep separate helpers here.
// ---------------------------------------------------------------------------

// Encode a unit direction to [-1,1]^2 using octahedral projection.
vec2 DDGIOctEncode(vec3 dir)
{
    dir /= (abs(dir.x) + abs(dir.y) + abs(dir.z));
    if (dir.z < 0.0f)
    {
        vec2 s = sign(dir.xy);
        s = mix(vec2(-1.0f), vec2(1.0f), greaterThanEqual(s, vec2(0.0f)));
        dir.xy = (1.0f - abs(dir.yx)) * s;
    }
    return dir.xy;
}

// Decode [-1,1]^2 to a unit direction.
vec3 DDGIOctDecode(vec2 uv)
{
    vec3 n = vec3(uv, 1.0f - abs(uv.x) - abs(uv.y));
    if (n.z < 0.0f)
    {
        vec2 s = sign(n.xy);
        s = mix(vec2(-1.0f), vec2(1.0f), greaterThanEqual(s, vec2(0.0f)));
        n.xy = (1.0f - abs(n.yx)) * s;
    }
    return normalize(n);
}

// ---------------------------------------------------------------------------
// Atlas UV helpers
// ---------------------------------------------------------------------------

// World-space position of probe at integer grid index.
vec3 DDGIProbeWorldPos(ivec3 probeIdx, DDGIData d)
{
    return d.gridOrigin + vec3(probeIdx) * d.probeSpacing;
}

// Map a probe linear index + octahedral direction to an atlas UV in [0,1].
// borderSize = 1 (one texel border around each probe's octahedral patch).
vec2 DDGIIrradianceAtlasUV(uint probeLinearIdx, vec3 dir, DDGIData d)
{
    const uint borderSize     = 1u;
    const uint probeTexels    = d.irradianceProbeSize;            // e.g. 8
    const uint probePitch     = probeTexels + 2u * borderSize;   // e.g. 10
    const uint probesPerRow   = uint(d.irradianceAtlasSize.x) / probePitch;

    uvec2 probeAtlasCoord = uvec2(
        (probeLinearIdx % probesPerRow) * probePitch,
        (probeLinearIdx / probesPerRow) * probePitch);

    // Octahedral coordinate in [0,1] within the probe's patch
    vec2 octNorm = DDGIOctEncode(normalize(dir)) * 0.5f + 0.5f; // [-1,1] -> [0,1]

    // Map octNorm [0,1] to inner texel centers [0.5, probeTexels-0.5].
    // Keeps bilinear sampling strictly within the inner region, away from the black border.
    vec2 innerOffset = 0.5f + octNorm * (float(probeTexels) - 1.0f);

    vec2 innerUV = (vec2(probeAtlasCoord) + float(borderSize) + innerOffset)
                   / d.irradianceAtlasSize;
    return innerUV;
}

vec2 DDGIVisibilityAtlasUV(uint probeLinearIdx, vec3 dir, DDGIData d)
{
    const uint borderSize     = 1u;
    const uint probeTexels    = d.visibilityProbeSize;            // e.g. 16
    const uint probePitch     = probeTexels + 2u * borderSize;   // e.g. 18
    const uint probesPerRow   = uint(d.visibilityAtlasSize.x) / probePitch;

    uvec2 probeAtlasCoord = uvec2(
        (probeLinearIdx % probesPerRow) * probePitch,
        (probeLinearIdx / probesPerRow) * probePitch);

    vec2 octNorm = DDGIOctEncode(normalize(dir)) * 0.5f + 0.5f;

    vec2 innerOffset = 0.5f + octNorm * (float(probeTexels) - 1.0f);

    vec2 innerUV = (vec2(probeAtlasCoord) + float(borderSize) + innerOffset)
                   / d.visibilityAtlasSize;
    return innerUV;
}

// Probe linear index from 3-D index (clamped to grid bounds).
uint DDGIProbeLinearIndex(ivec3 probeIdx, DDGIData d)
{
    probeIdx = clamp(probeIdx, ivec3(0), ivec3(d.probeCountX - 1u, d.probeCountY - 1u, d.probeCountZ - 1u));
    return uint(probeIdx.x)
         + uint(probeIdx.y) * d.probeCountX
         + uint(probeIdx.z) * d.probeCountX * d.probeCountY;
}

// ---------------------------------------------------------------------------
// Probe offset atlas helpers
//
// The offset atlas stores one texel per probe (no border), packed into the
// same row-major square layout as the irradiance/visibility atlases.
// ---------------------------------------------------------------------------

ivec2 DDGIProbeOffsetTexel(uint probeLinearIdx, DDGIData d)
{
    uint totalProbes  = d.probeCountX * d.probeCountY * d.probeCountZ;
    uint probesPerRow = max(1u, uint(ceil(sqrt(float(totalProbes)))));
    return ivec2(probeLinearIdx % probesPerRow, probeLinearIdx / probesPerRow);
}

// World-space probe position with per-probe relocation offset applied.
vec3 DDGIProbeWorldPosOffset(ivec3 probeIdx, DDGIData d, sampler2D offsetAtlas)
{
    vec3 base      = DDGIProbeWorldPos(probeIdx, d);
    uint linearIdx = DDGIProbeLinearIndex(probeIdx, d);
    vec3 offset    = texelFetch(offsetAtlas, DDGIProbeOffsetTexel(linearIdx, d), 0).xyz;
    return base + offset;
}

// ---------------------------------------------------------------------------
// Chebyshev visibility weight
// ---------------------------------------------------------------------------

float DDGIChebyshevVisibility(vec2 visData, float dist)
{
    // Uninitialized visibility atlas has (0,0) — treat as fully visible to avoid
    // zeroing out all probe weights on the first frames.
    if (visData.x < 1e-3f && visData.y < 1e-3f) return 1.0f;

    float meanDist   = visData.x;
    float meanSqDist = visData.y;
    if (dist <= meanDist) return 1.0f;

    float variance = max(meanSqDist - meanDist * meanDist, 1e-6f);
    float d        = dist - meanDist;
    float pMax     = variance / (variance + d * d);

    // Crush light bleeding: shift the curve up so probes clearly behind walls
    // fall to zero faster. Higher crush = less leaking, more loss of soft shadowing.
    const float crush = 0.4f;
    pMax = max((pMax - crush) / (1.0f - crush), 0.0f);

    // Cube the result for a sharper binary visible/occluded transition.
    return pMax * pMax * pMax;
}

// ---------------------------------------------------------------------------
// Sample irradiance from a single probe in the direction of `dir`
// ---------------------------------------------------------------------------

vec3 DDGISampleProbeIrradiance(
    ivec3 probeIdx,
    vec3  dir,
    DDGIData d,
    sampler2D irradianceAtlas)
{
    uint linearIdx = DDGIProbeLinearIndex(probeIdx, d);
    vec2 uv = DDGIIrradianceAtlasUV(linearIdx, dir, d);
    return texture(irradianceAtlas, uv).rgb;
}

// ---------------------------------------------------------------------------
// SampleDDGIIrradiance
//
// Trilinearly interpolates across the 8 surrounding probes, weighting by:
//   - Trilinear alpha
//   - Backface weight (probe behind the surface contributes less)
//   - Chebyshev visibility (probes occluded by geometry down-weighted)
// ---------------------------------------------------------------------------

vec3 SampleDDGIIrradiance(
    vec3 worldPos,
    vec3 worldNormal,
    DDGIData d,
    sampler2D irradianceAtlas,
    sampler2D visibilityAtlas,
    sampler2D offsetAtlas)
{
    if (d.isEnabled == 0) return vec3(0.0f);

    // Grid coordinate of worldPos
    vec3 gridCoord = (worldPos - d.gridOrigin) / d.probeSpacing;
    ivec3 baseIdx  = ivec3(floor(gridCoord));
    vec3  frac     = fract(gridCoord);

    vec3  accumIrradiance = vec3(0.0f);
    float accumWeight     = 0.0f;

    for (int iz = 0; iz < 2; iz++)
    for (int iy = 0; iy < 2; iy++)
    for (int ix = 0; ix < 2; ix++)
    {
        ivec3 probeIdx = baseIdx + ivec3(ix, iy, iz);

        // Skip probes outside grid
        if (any(lessThan(probeIdx, ivec3(0))) ||
            any(greaterThanEqual(probeIdx, ivec3(d.probeCountX, d.probeCountY, d.probeCountZ))))
        {
            continue;
        }

        vec3 probePos = DDGIProbeWorldPosOffset(probeIdx, d, offsetAtlas);

        // Trilinear weight
        vec3 trilinear = mix(1.0f - frac, frac, vec3(ix, iy, iz));
        float weight   = trilinear.x * trilinear.y * trilinear.z;

        // Backface weight: exclude probes behind the surface.
        // Uses a hard cosine lobe — no 0.5 shift, so probes at 90° and beyond
        // get weight ≈ 0 and don't leak through walls.
        vec3 probeDir = normalize(worldPos - probePos);
        float backfaceWeight = max(0.0f, dot(worldNormal, -probeDir));
        backfaceWeight = backfaceWeight * backfaceWeight + 0.02f;
        weight *= backfaceWeight;

        // Chebyshev visibility
        float dist = length(worldPos - probePos);
        uint linearIdx = DDGIProbeLinearIndex(probeIdx, d);
        vec2 visUV = DDGIVisibilityAtlasUV(linearIdx, probeDir, d);
        vec2 visData = texture(visibilityAtlas, visUV).rg;
        float visWeight = DDGIChebyshevVisibility(visData, dist);
        weight *= visWeight;

        weight = max(weight, 1e-6f);

        // Sample irradiance in the world-normal direction
        vec3 irradiance = DDGISampleProbeIrradiance(probeIdx, worldNormal, d, irradianceAtlas);
        accumIrradiance += irradiance * weight;
        accumWeight     += weight;
    }

    if (accumWeight < 1e-6f) return vec3(0.0f);
    return accumIrradiance / accumWeight;
}

#endif
