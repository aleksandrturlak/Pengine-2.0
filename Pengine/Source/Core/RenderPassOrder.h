#pragma once

#include <string>
#include <vector>

namespace Pengine
{

	const std::vector<std::string> passPerSceneOrder =
	{
		Atmosphere
	};

	const std::vector<std::string> passPerViewportOrder =
	{
		UI,
		ZPrePass,
		ComputeIndirectDrawGBuffer,
		GBuffer,
		Decals,
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
		SSRBlur,
		Bloom,
		ToneMapping,
		AntiAliasingAndCompose,
	};

}
