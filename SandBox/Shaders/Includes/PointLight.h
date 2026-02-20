#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

struct PointLightFaceInfo
{
	mat4 viewProjectionMat4;
};

struct PointLight
{
	PointLightFaceInfo pointLightFaceInfos[6];
	
	vec3 color;
	float intensity;
	
	vec3 positionViewSpace;
	float radius;

	vec3 positionWorldSpace;
	int shadowMapIndex;

	float bias;
	
    int castSSS;
};

struct PointLightShadows
{
	int isEnabled;
	int shadowMapAtlasSize;
	int faceSize;
};

vec3 CalculatePointLight(
	in PointLight light,
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

	vec3 H = normalize(viewDirectionViewSpace + directionViewSpace);

	vec3 radiance = light.color * light.intensity;

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

vec2 DirectionToCubemapUV(vec3 direction, uint faceIndex)
{   
    vec3 absDirection = abs(direction);
    float ma;
    vec2 uv;
    
    if (faceIndex == 0)
	{
        ma = absDirection.x;
        uv = vec2(-direction.z, direction.y) / ma;
    }
	else if (faceIndex == 1)
	{
        ma = absDirection.x;
        uv = vec2(direction.z, direction.y) / ma;
    }
	else if (faceIndex == 2)
	{
        ma = absDirection.y;
        uv = vec2(-direction.x, direction.z) / ma;
    }
	else if (faceIndex == 3)
	{
        ma = absDirection.y;
        uv = vec2(-direction.x, -direction.z) / ma;
    }
	else if (faceIndex == 4)
	{
        ma = absDirection.z;
        uv = vec2(direction.x, direction.y) / ma;
    }
	else
	{
        ma = absDirection.z;
        uv = vec2(-direction.x, direction.y) / ma;
    }
    
    return uv * 0.5 + 0.5;
}

vec2 GetShadowFaceUVLinear(
	in PointLightShadows pointLightShadows,
    uint shadowMapIndex,
    uint faceIndex,
    vec3 direction)
{
    uint totalFaceIndex = shadowMapIndex * 6 + faceIndex;
    uint facesPerRow = pointLightShadows.shadowMapAtlasSize / pointLightShadows.faceSize;

    uint faceRow = totalFaceIndex / facesPerRow;
    uint faceCol = totalFaceIndex % facesPerRow;

    vec2 faceBase = vec2(
        float(faceCol * pointLightShadows.faceSize),
        float(faceRow * pointLightShadows.faceSize)
    );
    
    vec2 uv = DirectionToCubemapUV(direction, faceIndex);
    vec2 pixelCoords = faceBase + uv * vec2(float(pointLightShadows.faceSize));
    
    return vec2(
        pixelCoords.x / float(pointLightShadows.shadowMapAtlasSize),
        pixelCoords.y / float(pointLightShadows.shadowMapAtlasSize)
    );
}

uint GetFaceIndexFromDirection(vec3 direction)
{
    vec3 absDirection = abs(direction);
    float maxComponent = max(max(absDirection.x, absDirection.y), absDirection.z);
    
    if (maxComponent == absDirection.x)
	{
        return (direction.x > 0.0) ? 0u : 1u;
    }
	else if (maxComponent == absDirection.y)
	{
        return (direction.y > 0.0) ? 2u : 3u;
    }
	else
	{
        return (direction.z > 0.0) ? 4u : 5u;
    }
}

void GetFaceUVBounds(
    int shadowMapIndex,
	int faceIndex,
    int atlasSize,
    int faceSize,
    out vec2 minUV,
    out vec2 maxUV)
{
	int totalFaceIndex = shadowMapIndex * 6 + faceIndex;

    int facesPerRow = atlasSize / faceSize;
    int faceRow = totalFaceIndex / facesPerRow;
    int faceCol = totalFaceIndex % facesPerRow;
    
    float pixelX = float(faceCol * faceSize);
    float pixelY = float(faceRow * faceSize);
    
    minUV = vec2((pixelX) / float(atlasSize), (pixelY) / float(atlasSize));
    maxUV = vec2((pixelX - 1 + float(faceSize)) / float(atlasSize),
                 (pixelY - 1 + float(faceSize)) / float(atlasSize));
}

float CalculatePointLightShadow(
	in sampler2D shadowAtlasTexture,
    in PointLight light,
	in PointLightShadows pointLightShadows,
    in vec3 toLight,
    float distanceToPoint)
{
	float shadow = 0.0f;

    vec2 texelSize = 1.0f / vec2(float(pointLightShadows.shadowMapAtlasSize));

	float random = fract(sin(dot(vec2(distanceToPoint, toLight.x) * texelSize, vec2(12.9898, 78.233))) * 43758.5453);
	float angle = random * 2.0 * 3.14159265;
	
	mat2 rotation = mat2(cos(angle), -sin(angle),
						sin(angle), cos(angle));

	vec3 direction = normalize(toLight);
	uint faceIndex = GetFaceIndexFromDirection(direction);
	
	vec2 atlasUV = GetShadowFaceUVLinear(pointLightShadows, light.shadowMapIndex, faceIndex, direction);

	vec2 minUV;
	vec2 maxUV;
	GetFaceUVBounds(
		light.shadowMapIndex,
		int(faceIndex),
    	pointLightShadows.shadowMapAtlasSize,
    	pointLightShadows.faceSize,
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
