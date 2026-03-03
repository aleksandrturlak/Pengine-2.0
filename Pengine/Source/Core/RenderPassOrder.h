#pragma once

#include <string>
#include <vector>

namespace Pengine
{

	const std::vector<std::string> passPerSceneOrder =
	{
		GPUSkinning,
		Atmosphere
	};

	const std::vector<std::string> passPerViewportOrder =
	{
		UI,
		ComputeIndirectDrawGBuffer,
		GBuffer,
		HiZPyramid,
		Decals,
		ComputeIndirectDrawCSM,
		CSM,
		PointLightShadows,
		SpotLightShadows,
		SSAO,
		SSAOBlur,
		SSS,
		SSSBlur,
		Deferred,
		Transparent,
		SSR,
		RayTracedReflection,
		BlurReflections,
		Bloom,
		ToneMapping,
		AntiAliasingAndCompose,
	};

}
