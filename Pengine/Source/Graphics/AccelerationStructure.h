#pragma once

#include "../Core/Core.h"

namespace Pengine
{

	class Mesh;

	class PENGINE_API AccelerationStructure
	{
	public:
		
		enum class GeometryInstanceFlagBits : uint32_t
		{
			TRIANGLE_FACING_CULL_DISABLE_BIT = 0x00000001,
			TRIANGLE_FLIP_FACING_BIT = 0x00000002,
			FORCE_OPAQUE_BIT = 0x00000004,
			FORCE_NO_OPAQUE_BIT = 0x00000008,
			FORCE_OPACITY_MICROMAP_2_STATE_EXT = 0x00000010,
			DISABLE_OPACITY_MICROMAPS_EXT = 0x00000020,
			TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT = TRIANGLE_FLIP_FACING_BIT,
			FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
		};
		typedef uint32_t GeometryInstanceFlags;

		struct Instance
		{
			float transform[3][4];
			uint32_t instanceCustomIndex:24;
			uint32_t mask:8;
			uint32_t instanceShaderBindingTableRecordOffset:24;
			GeometryInstanceFlags flags:8;
			uint64_t accelerationStructureReference;
		};

		virtual ~AccelerationStructure() = default;
		AccelerationStructure(const AccelerationStructure&) = delete;
		AccelerationStructure& operator=(const AccelerationStructure&) = delete;

		[[nodiscard]] virtual uint64_t GetDeviceAddress() const = 0;

		static std::shared_ptr<AccelerationStructure> CreateBLAS(const Mesh& mesh);

		static std::shared_ptr<AccelerationStructure> CreateTLAS(
			const std::vector<Instance>& instances,
			void* frame);

	protected:
		AccelerationStructure() = default;
	};

}
