#ifndef DISSOLVE_MATERIAL_H
#define DISSOLVE_MATERIAL_H

// Superset of DefaultMaterial — first fields are identical so shadow passes
// (which cast to DefaultMaterial) read correct data without modification.
struct DissolveMaterial
{
	// --- DefaultMaterial fields (must stay in this exact order) ---
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

	// --- Dissolve-specific fields ---
	float dissolveProgress;   // 0 = fully solid, 1 = fully dissolved
	float dissolveEdgeWidth;  // noise threshold width for the edge glow
	float dissolveNoiseSeed;  // per-enemy random offset for unique patterns
	vec4  dissolveEdgeColor;  // emissive color of the dissolve edge
};

#endif
