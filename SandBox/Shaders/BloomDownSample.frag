#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;

layout(set = 0, binding = 1) uniform MipBuffer
{
    vec2 sourceSize;
    int mipLevel;
    float bloomIntensity;
};

void main()
{
    float intensity = mipLevel == 0 ? bloomIntensity : 1.0f;

    vec2 srcTexelSize = 1.0f / sourceSize;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(sourceTexture, vec2(uv.x - 2*x, uv.y + 2*y)).rgb * intensity;
    vec3 b = texture(sourceTexture, vec2(uv.x,       uv.y + 2*y)).rgb * intensity;
    vec3 c = texture(sourceTexture, vec2(uv.x + 2*x, uv.y + 2*y)).rgb * intensity;

    vec3 d = texture(sourceTexture, vec2(uv.x - 2*x, uv.y)).rgb * intensity;
    vec3 e = texture(sourceTexture, vec2(uv.x,       uv.y)).rgb * intensity;
    vec3 f = texture(sourceTexture, vec2(uv.x + 2*x, uv.y)).rgb * intensity;

    vec3 g = texture(sourceTexture, vec2(uv.x - 2*x, uv.y - 2*y)).rgb * intensity;
    vec3 h = texture(sourceTexture, vec2(uv.x,       uv.y - 2*y)).rgb * intensity;
    vec3 i = texture(sourceTexture, vec2(uv.x + 2*x, uv.y - 2*y)).rgb * intensity;

    vec3 j = texture(sourceTexture, vec2(uv.x - x, uv.y + y)).rgb * intensity;
    vec3 k = texture(sourceTexture, vec2(uv.x + x, uv.y + y)).rgb * intensity;
    vec3 l = texture(sourceTexture, vec2(uv.x - x, uv.y - y)).rgb * intensity;
    vec3 m = texture(sourceTexture, vec2(uv.x + x, uv.y - y)).rgb * intensity;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving outColor we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    outColor.xyz = e * 0.125f;
    outColor.xyz += (a + c + g + i) * 0.03125f;
    outColor.xyz += (b + d + f + h) * 0.0625f;
    outColor.xyz += (j + k + l + m) * 0.125f;
}
