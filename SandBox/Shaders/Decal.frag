#version 450

#include "Shaders/Includes/Common.h"

layout(location = 0) flat in uint64_t materialBuffer;
layout(location = 1) flat in mat4 inverseTransform;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outShading;
layout(location = 2) out vec4 outEmissive;

#include "Shaders/Includes/SetMacros/CameraSet.h"
CAMERA_SET(0)

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[MAX_BINDLESS_TEXTURES];

layout(set = 2, binding = 0) uniform sampler2D depthGBufferTexture;
layout(set = 2, binding = 1, rg16f) uniform image2D normalGBufferTexture;

#include "Shaders/Includes/DefaultMaterial.h"
layout(set = 3, binding = 0, scalar) uniform MaterialBuffer
{
	DefaultMaterial material;
};

layout(buffer_reference, scalar) buffer MaterialBufferReference
{
	DefaultMaterial material;
};

#include "Shaders/Includes/ParallaxOcclusionMapping.h"

mat3 ConstructTBN( vec3 N, vec3 p, vec2 uv )
{
	vec3 dp1 = dFdx( p );
	vec3 dp2 = dFdy( p );
	vec2 duv1 = dFdx( uv );
	vec2 duv2 = dFdy( uv );
	vec3 dp2perp = cross( dp2, N );
	vec3 dp1perp = cross( N, dp1 );
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
	float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
	return mat3( T * invmax, B * invmax, N );
}

void main()
{
	DefaultMaterial material = MaterialBufferReference(materialBuffer).material;
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    vec2 screenUV = (vec2(pixelCoord) + vec2(0.5f)) / camera.viewportSize;
	vec2 screenPosition = screenUV * 2.0f - 1.0f;
	
    vec2 viewRay = ComputeViewRay(screenPosition, vec2(0.0f), camera.aspectRatio, camera.tanHalfFOV);

    vec3 positionViewSpace = CalculatePositionFromDepth(
       texture(depthGBufferTexture, screenUV).x,
       camera.projectionMat4,
       viewRay);

    vec3 positionWorldSpace = (camera.inverseViewMat4 * vec4(positionViewSpace, 1.0f)).xyz;
    vec3 localPositionWorldSpace = vec4(inverseTransform * vec4(positionWorldSpace, 1.0f)).xyz;
    vec2 decalUV = localPositionWorldSpace.xz * 0.5f + 0.5f;

    if (abs(localPositionWorldSpace.x) > 1.0f || abs(localPositionWorldSpace.z) > 1.0f ||
       abs(localPositionWorldSpace.y) > 1.0f)
    {
		discard;
	}

	vec3 gbufferNormalViewSpace = OctDecode(imageLoad(normalGBufferTexture, pixelCoord).xy);
	
	vec2 uvForTBN = decalUV;
	if (material.useParallaxOcclusion > 0)
	{
		vec3 normalWorldSpace = normalize(mat3(camera.inverseViewMat4) * normalize(gbufferNormalViewSpace));
		mat3 TBN = transpose(ConstructTBN(normalWorldSpace, positionWorldSpace, uvForTBN));

		vec3 cameraPositionTangentSpace = TBN * camera.positionWorldSpace;
    	vec3 positionTangentSpace = TBN * positionWorldSpace;
		vec3 viewDirectionTangentSpace = normalize(cameraPositionTangentSpace - positionTangentSpace);
		decalUV = ParallaxOcclusionMapping(
			bindlessTextures[material.heightTexture],
			decalUV,
			viewDirectionTangentSpace,
			material.minParallaxLayers,
			material.maxParallaxLayers,
			-material.parallaxHeightScale);
	}

	vec4 albedoColor = texture(bindlessTextures[material.albedoTexture], decalUV) * material.albedoColor;
	if (material.useAlphaCutoff > 0)
	{
		if (albedoColor.a < material.alphaCutoff)
		{
			discard;
		}
	}

	vec3 metallicRoughness = texture(bindlessTextures[material.metallicRoughnessTexture], decalUV).xyz;
	float metallic = metallicRoughness.b;
	float roughness = metallicRoughness.g;
	float ao = texture(bindlessTextures[material.aoTexture], decalUV).r;

	outAlbedo = albedoColor;
	outShading = vec4(
		metallic * material.metallicFactor,
		roughness * material.roughnessFactor,
		ao * material.aoFactor,
		1.0f);
	outEmissive = texture(bindlessTextures[material.emissiveTexture], decalUV) * material.emissiveColor * material.emissiveFactor;
	
	if (material.useNormalMap > 0)
	{
		mat3 TBN = ConstructTBN(gbufferNormalViewSpace, positionViewSpace, uvForTBN);
		vec3 normalMap = texture(bindlessTextures[material.normalTexture], decalUV).xyz;
		normalMap = normalMap * 2.0f - 1.0f;
		imageStore(normalGBufferTexture, pixelCoord, vec4(OctEncode(normalize(TBN * normalMap)), 0.0f, 0.0f));
	}
	else
	{
		imageStore(normalGBufferTexture, pixelCoord, vec4(OctEncode(gbufferNormalViewSpace), 0.0f, 0.0f));
	}
}
