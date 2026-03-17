#pragma once

#include "Core.h"
#include "CustomData.h"

#include "../Graphics/Texture.h"
#include "../Graphics/Buffer.h"

#include <glm/glm.hpp>
#include <limits>

namespace Pengine
{

	class PENGINE_API DDGIRenderer : public CustomData
	{
	public:
		// Layout must match DDGIData struct in DDGI.h
		struct DDGIData
		{
			glm::vec3 gridOrigin;
			uint32_t  probeCountX = 0;

			uint32_t  probeCountY = 0;
			uint32_t  probeCountZ = 0;
			float     probeSpacing = 1.0f;
			uint32_t  raysPerProbe = 64;

			float     hysteresisIrradiance = 0.97f;
			float     hysteresisVisibility = 0.90f;
			glm::vec2 irradianceAtlasSize;

			glm::vec2 visibilityAtlasSize;
			uint32_t  irradianceProbeSize = 8;
			uint32_t  visibilityProbeSize = 16;

			int32_t   isEnabled = 0;
			uint32_t  frameIndex = 0;
		};

		glm::uvec3 m_GridDimensions = { 16, 4, 16 };
		glm::vec3 m_LastGridOrigin = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 m_FixedOrigin = glm::vec3(0.0f);
		float m_ProbeSpacing = 2.0f;
		uint32_t m_RaysPerProbe = 64;
		uint32_t m_IrradianceProbeSize = 8;
		uint32_t m_VisibilityProbeSize = 16;
		uint32_t m_FrameIndex = 0;
		bool m_IsEnabled = false;
		bool m_FollowCamera = true;
		std::shared_ptr<Texture> m_ProbeOffsetAtlas;

		// Persistent probe atlases (R/W every frame after blend)
		std::shared_ptr<Texture> m_IrradianceAtlas; // rgba16f
		std::shared_ptr<Texture> m_VisibilityAtlas; // rg16f

		// Temporary atlases written each frame by DDGIProbeUpdate
		std::shared_ptr<Texture> m_TempIrradianceAtlas;
		std::shared_ptr<Texture> m_TempVisibilityAtlas;
		std::shared_ptr<Buffer> m_DDGIBuffer;

		glm::ivec2 GetIrradianceAtlasSize() const
		{
			const uint32_t pitch = m_IrradianceProbeSize + 2u; // +2 border
			const uint32_t totalProbes = m_GridDimensions.x * m_GridDimensions.y * m_GridDimensions.z;
			// Pack probes into a square-ish atlas
			const uint32_t probesPerRow = std::max(1u, (uint32_t)std::ceil(std::sqrt((float)totalProbes)));
			const uint32_t rows = (totalProbes + probesPerRow - 1u) / probesPerRow;
			return glm::ivec2(probesPerRow * pitch, rows * pitch);
		}

		glm::ivec2 GetVisibilityAtlasSize() const
		{
			const uint32_t pitch = m_VisibilityProbeSize + 2u;
			const uint32_t totalProbes = m_GridDimensions.x * m_GridDimensions.y * m_GridDimensions.z;
			const uint32_t probesPerRow = std::max(1u, (uint32_t)std::ceil(std::sqrt((float)totalProbes)));
			const uint32_t rows = (totalProbes + probesPerRow - 1u) / probesPerRow;
			return glm::ivec2(probesPerRow * pitch, rows * pitch);
		}

		glm::ivec2 GetProbeOffsetAtlasSize() const
		{
			const uint32_t totalProbes = m_GridDimensions.x * m_GridDimensions.y * m_GridDimensions.z;
			const uint32_t probesPerRow = std::max(1u, (uint32_t)std::ceil(std::sqrt((float)totalProbes)));
			const uint32_t rows = (totalProbes + probesPerRow - 1u) / probesPerRow;
			return glm::ivec2(probesPerRow, rows);
		}

		glm::vec3 ComputeGridOrigin(const glm::vec3& cameraWorldPos) const
		{
			glm::vec3 halfExtent = glm::vec3(m_GridDimensions - glm::uvec3(1)) * 0.5f * m_ProbeSpacing;

			if (!m_FollowCamera)
				return m_FixedOrigin - halfExtent;

			return glm::floor((cameraWorldPos - halfExtent) / m_ProbeSpacing) * m_ProbeSpacing;
		}

		DDGIData BuildShaderData(const glm::vec3& cameraWorldPos) const
		{
			glm::vec3 snappedOrigin = ComputeGridOrigin(cameraWorldPos);

			const glm::ivec2 irrSize = GetIrradianceAtlasSize();
			const glm::ivec2 visSize = GetVisibilityAtlasSize();

			DDGIData data{};
			data.gridOrigin            = snappedOrigin;
			data.probeCountX           = m_GridDimensions.x;
			data.probeCountY           = m_GridDimensions.y;
			data.probeCountZ           = m_GridDimensions.z;
			data.probeSpacing          = m_ProbeSpacing;
			data.raysPerProbe          = m_RaysPerProbe;
			data.hysteresisIrradiance  = 0.97f;
			data.hysteresisVisibility  = 0.90f;
			data.irradianceAtlasSize   = glm::vec2(irrSize);
			data.visibilityAtlasSize   = glm::vec2(visSize);
			data.irradianceProbeSize   = m_IrradianceProbeSize;
			data.visibilityProbeSize   = m_VisibilityProbeSize;
			data.isEnabled             = m_IsEnabled ? 1 : 0;
			data.frameIndex            = m_FrameIndex;
			return data;
		}
	};

}
