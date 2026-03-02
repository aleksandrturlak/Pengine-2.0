#pragma once

#include "Core.h"
#include "Asset.h"

namespace Pengine
{

	class PENGINE_API GraphicsSettings : public Asset
	{
	public:
		GraphicsSettings(
			const std::string& name,
			const std::filesystem::path& filepath)
			: Asset(name, filepath)
		{}

		struct RayTracing
		{
			struct Shadows
			{
				bool directionalLight = false;
				bool pointLight = false;
				bool spotLight = false;
			} shadows;

			struct Reflections
			{
				bool isRayTraced = false;
			} reflections;
		} rayTracing;

		struct HiZOcclusionCulling
		{
			bool isEnabled = true;
			float depthBias = 0.025f;
		} hiZOcclusionCulling;

		struct SSAO
		{
			bool isEnabled = true;
			int kernelSize = 16;
			int noiseSize = 4;
			float radius = 0.5f;
			float bias = 0.025f;
			float aoScale = 2.0f;

			/**
			 * Resolution scale: 0.25 / 0.5 / 0.75 / 1.0
			 * 0.75 by default.
			 */
			int resolutionScale = 2;

			/**
			 * Resolution scale: 0.25 / 0.5 / 0.75 / 1.0
			 * 0.75 by default.
			 */
			int resolutionBlurScale = 2;
		} ssao;

		struct Shadows
		{
			struct CSM
			{
				enum class Filter : int
				{
					None = 0,
					PCF = 1,
					PoissonDisk = 2
				};

				bool isEnabled = true;
				std::vector<float> biases = { 0.0f, 0.0f, 0.0f };
				float splitFactor = 0.75f;
				float maxDistance = 200.0f;
				float fogFactor = 0.2f;
				Filter filter = Filter::PoissonDisk;
				bool stabilizeCascades = true;
				bool visualize = false;
				int pcfRange = 1;

				/**
				 * Resolution: 1024 / 2048 / 4096
				 * 1024 by default.
				 */
				int quality = 1;

				/**
				 * Has to be more than 1.
				 */
				int cascadeCount = 3;
			} csm;
			
			struct SSS
			{
				/**
				 * Resolution scale: 0.25 / 0.5 / 0.75 / 1.0
				 * 0.75 by default.
				 */
				int resolutionScale = 2;

				/**
				 * Resolution scale: 0.25 / 0.5 / 0.75 / 1.0
				 * 0.75 by default.
				 */
				int resolutionBlurScale = 2;

				bool isEnabled = true;
				int maxSteps = 16;
				float maxRayDistance = 0.1f;
				float maxDistance = 5.0f;
				float minThickness = -0.0125f;
				float maxThickness = 0.00015f;
			} sss;

			struct PointLightShadows
			{
				/**
				 * Resolution: 1024 / 2048 / 3072 / 4096
				 * 3072 by default.
				 */
				int atlasQuality = 2;

				/**
				 * Resolution: 128 / 256 / 512 / 1024
				 * 512 by default.
				 */
				int faceQuality = 2;
				
				bool isEnabled = true;
			} pointLightShadows;

			struct SpotLightShadows
			{
				/**
				 * Resolution: 1024 / 2048 / 3072 / 4096
				 * 3072 by default.
				 */
				int atlasQuality = 2;

				/**
				 * Resolution: 128 / 256 / 512 / 1024
				 * 512 by default.
				 */
				int faceQuality = 2;

				bool isEnabled = true;
			} spotLightShadows;
		} shadows;

		struct Bloom
		{
			bool isEnabled = true;
			int mipCount = 10;
			float brightnessThreshold = 1.0f;
			float intensity = 1.0f;

			/**
			 * Resolution scale: 0.25 / 0.5 / 0.75 / 1.0
			 * 1.0 by default.
			 */
			int resolutionScale = 3;
		} bloom;

		struct PostProcess
		{
			enum class ToneMapper : int
			{
				NONE,
				ACES,
				COUNT
			};

			ToneMapper toneMapper = ToneMapper::ACES;
			float gamma = 2.2f;
			bool fxaa = true;
		} postProcess;

		struct SSR
		{
			enum class Blur
			{
				NONE,
				SIMPLE,
				BILATERAL
			};

			bool isEnabled = true;
			bool useSkyBoxFallback = true;

			Blur blur = Blur::SIMPLE;

			int blurRange = 2;
			int blurOffset = 1;

			float maxDistance = 30.0f;
			float resolution = 0.3f;
			float thickness = 5.0f;
			int stepCount = 10;
			float mipMultiplier = 1.0f;

			/**
			 * Resolution scale: 0.25 / 0.5 / 0.75 / 1.0
			 * 0.5 by default.
			 */
			int resolutionScale = 1;

			/**
			 * Resolution scale: 0.125, 0.25 / 0.5 / 0.75 / 1.0
			 * 0.125 by default.
			 */
			int resolutionBlurScale = 1;
		} ssr;
	};

}
