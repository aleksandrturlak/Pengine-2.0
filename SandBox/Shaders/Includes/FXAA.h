#ifndef FXAA_H
#define FXAA_H

#ifndef FXAA_REDUCE_MIN
    #define FXAA_REDUCE_MIN   (1.0/ 128.0)
#endif
#ifndef FXAA_REDUCE_MUL
    #define FXAA_REDUCE_MUL   (1.0 / 8.0)
#endif
#ifndef FXAA_SPAN_MAX
    #define FXAA_SPAN_MAX     8.0
#endif

vec3 FXAA(
    sampler2D sourceTexture,
    vec2 fragmentPosition,
    vec2 viewportSize,
    vec2 v_rgbNW,
    vec2 v_rgbNE,
    vec2 v_rgbSW,
    vec2 v_rgbSE,
    vec2 v_rgbM)
{
	vec2 inverseViewport = 1.0f / viewportSize.xy;
    vec3 rgbNW = texture(sourceTexture, v_rgbNW).xyz;
    vec3 rgbNE = texture(sourceTexture, v_rgbNE).xyz;
    vec3 rgbSW = texture(sourceTexture, v_rgbSW).xyz;
    vec3 rgbSE = texture(sourceTexture, v_rgbSE).xyz;
    vec4 sourceColor = texture(sourceTexture, v_rgbM);
    vec3 rgbM = sourceColor.xyz;
    vec3 luma = vec3(0.299f, 0.587f, 0.114f);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    vec2 direction;
    direction.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    direction.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float directionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
                          (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    
    float rcpDirectionMin = 1.0f / (min(abs(direction.x), abs(direction.y)) + directionReduce);
    direction = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
              max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
              direction * rcpDirectionMin)) * inverseViewport;
    
    vec3 rgbA = 0.5f * (
        texture(sourceTexture, fragmentPosition * inverseViewport + direction * (1.0f / 3.0f - 0.5f)).xyz +
        texture(sourceTexture, fragmentPosition * inverseViewport + direction * (2.0f / 3.0f - 0.5f)).xyz);
    vec3 rgbB = rgbA * 0.5f + 0.25f * (
        texture(sourceTexture, fragmentPosition * inverseViewport + direction * -0.5f).xyz +
        texture(sourceTexture, fragmentPosition * inverseViewport + direction * 0.5f).xyz);

    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        return rgbA;
    }
    else
    {
        return rgbB;
    }
}

#endif
