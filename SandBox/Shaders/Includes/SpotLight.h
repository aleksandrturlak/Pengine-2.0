#ifndef SPOT_LIGHT_H
#define SPOT_LIGHT_H

struct SpotLight
{
	mat4 viewProjectionMat4;
	
	vec3 color;
	float intensity;
	
	vec3 positionViewSpace;
	float radius;

	vec3 positionWorldSpace;
    int castSSS;

    vec3 directionViewSpace;
	float bias;

    float innerCutOff;
    float outerCutOff;
};

struct SpotLightShadows
{
	int isEnabled;
	int shadowMapAtlasSize;
	int faceSize;
	int isRayTraced;
};

vec3 CalculateSpotLight(
	in SpotLight light,
	in vec3 viewDirectionViewSpace,
	in vec3 positionViewSpace,
	in vec3 basicReflectivity,
	in vec3 normalViewSpace,
	in vec3 albedo,
	in float metallic,
	in float roughness,
	in float ao,
	in float shadow)
{
	vec3 directionViewSpace = normalize(light.positionViewSpace - positionViewSpace);
	float theta = dot(directionViewSpace, normalize(-light.directionViewSpace));
    float epsilon = light.innerCutOff - light.outerCutOff;
    float intensity = clamp((theta - cos(light.outerCutOff)) / epsilon, 0.0f, 1.0f);

    if (intensity > 0.0f)
    {
        vec3 H = normalize(viewDirectionViewSpace + directionViewSpace);

        vec3 radiance = light.color * light.intensity * intensity;

        float NdotV = max(dot(normalViewSpace, viewDirectionViewSpace), 0.0000001f);
        float NdotL = max(dot(normalViewSpace, directionViewSpace), 0.0000001f);
        float HdotV = max(dot(H, viewDirectionViewSpace), 0.0f);
        float NdotH = max(dot(normalViewSpace, H), 0.0f);

        float D = DistributionGGX(NdotH, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        vec3 F = FresnelSchlick(HdotV, basicReflectivity);

        vec3 specular = D * G * F;
        specular /= 4.0 * NdotV * NdotL;// + 0.0001;

        vec3 kS = F;
        vec3 kD = vec3(1.0f) - kS;

        kD *= 1.0f - metallic;
        
        float distance    = length(light.positionViewSpace - positionViewSpace);
        float attenuation = max(0.0f,
            (1.0f / (distance * distance)) - (1.0f / (light.radius * light.radius)));

        return (1.0f - shadow) * (kD * albedo / PI + specular) * radiance * NdotL * attenuation;
    }
	else
    {
        return vec3(0.0f, 0.0f, 0.0f);
    }
}

vec2 GetShadowFaceUVLinear(
	in SpotLightShadows spotLightShadows,
    uint shadowMapIndex)
{
    uint totalFaceIndex = shadowMapIndex;
    uint facesPerRow = spotLightShadows.shadowMapAtlasSize / spotLightShadows.faceSize;

    uint faceRow = totalFaceIndex / facesPerRow;
    uint faceCol = totalFaceIndex % facesPerRow;

    vec2 pixelCoords = vec2(
        float(faceCol * spotLightShadows.faceSize),
        float(faceRow * spotLightShadows.faceSize)
    );
    
    return vec2(
        pixelCoords.x / float(spotLightShadows.shadowMapAtlasSize),
        pixelCoords.y / float(spotLightShadows.shadowMapAtlasSize)
    );
}

void GetFaceUVBounds(
    int shadowMapIndex,
    int atlasSize,
    int faceSize,
    out vec2 minUV,
    out vec2 maxUV)
{
	int totalFaceIndex = shadowMapIndex;

    int facesPerRow = atlasSize / faceSize;
    int faceRow = totalFaceIndex / facesPerRow;
    int faceCol = totalFaceIndex % facesPerRow;
    
    float pixelX = float(faceCol * faceSize);
    float pixelY = float(faceRow * faceSize);
    
    minUV = vec2((pixelX) / float(atlasSize), (pixelY) / float(atlasSize));
    maxUV = vec2((pixelX - 1 + float(faceSize)) / float(atlasSize),
                 (pixelY - 1 + float(faceSize)) / float(atlasSize));
}

float CalculateSpotLightShadow(
	in sampler2D shadowAtlasTexture,
    in SpotLight light,
	in SpotLightShadows spotLightShadows,
    in vec3 positionWorldSpace,
    in vec3 positionViewSpace,
    float distanceToPoint,
    int shadowMapIndex)
{
	float shadow = 0.0f;

    vec3 directionViewSpace = normalize(light.positionViewSpace - positionViewSpace);
	
    float theta = dot(directionViewSpace, normalize(-light.directionViewSpace));
    float epsilon = light.innerCutOff - light.outerCutOff;
    float intensity = clamp((theta - cos(light.outerCutOff)) / epsilon, 0.0f, 1.0f);

    if (intensity <= 0.0f)
    {
        return shadow;
    }

	vec4 positionLightSpace = light.viewProjectionMat4 * vec4(positionWorldSpace, 1.0f);
	vec3 projectedCoords = positionLightSpace.xyz / positionLightSpace.w;

    vec2 texelSize = 1.0f / vec2(float(spotLightShadows.shadowMapAtlasSize));

	float random = fract(sin(dot(positionWorldSpace.xy * texelSize, vec2(12.9898, 78.233))) * 43758.5453);
	float angle = random * 2.0 * 3.14159265;
	
	mat2 rotation = mat2(cos(angle), -sin(angle),
						sin(angle), cos(angle));
	
    projectedCoords = (projectedCoords + 1.0f) / 2.0f;
    vec2 baseUV = ((projectedCoords.xy * float(spotLightShadows.faceSize)) / float(spotLightShadows.shadowMapAtlasSize));
	vec2 atlasUV = baseUV + GetShadowFaceUVLinear(spotLightShadows, shadowMapIndex);

	vec2 minUV;
	vec2 maxUV;
	GetFaceUVBounds(
		shadowMapIndex,
    	spotLightShadows.shadowMapAtlasSize,
    	spotLightShadows.faceSize,
    	minUV,
    	maxUV);

	for (int i = 0; i < 16; i++)
	{
		vec2 offset = rotation * poissonDisk[i];
		vec2 uv = atlasUV.xy + offset * texelSize;
		uv = clamp(uv, minUV, maxUV);
		float closestDepth = texture(shadowAtlasTexture, uv).r * light.radius;
		shadow += (distanceToPoint - light.bias) > closestDepth ? 1.0f : 0.0f;
	}
	shadow /= 16;

    return shadow;
}

#endif