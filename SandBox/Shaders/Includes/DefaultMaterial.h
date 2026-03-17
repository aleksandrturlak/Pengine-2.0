#ifndef DEFAULT_MATERIAL_H
#define DEFAULT_MATERIAL_H

struct DefaultMaterial
{
	vec4 albedoColor;
	vec4 emissiveColor;
	vec4 uvTransform;
	
	float metallicFactor;
	float roughnessFactor;
	float aoFactor;
	
	float emissiveFactor;
	float alphaCutoff;
	
	int maxParallaxLayers;
	int minParallaxLayers;
	float parallaxHeightScale;
	
	int useNormalMap;
	int useAlphaCutoff;
	int useParallaxOcclusion;

	int albedoTexture;
	int normalTexture;
	int metallicRoughnessTexture;
	int aoTexture;
	int emissiveTexture;
	int heightTexture;
};

#endif
