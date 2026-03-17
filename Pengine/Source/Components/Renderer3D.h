#pragma once

#include "../Core/Core.h"
#include "../Core/BoundingBox.h"
#include "../Core/UUID.h"
#include "../Graphics/AccelerationStructure.h"
#include "../Graphics/Buffer.h"

namespace Pengine
{

	class Mesh;
	class Material;

	class PENGINE_API Renderer3D
	{
	public:
		std::shared_ptr<Mesh> mesh;
		std::shared_ptr<Material> material;

		Renderer3D() = default;
		Renderer3D(const Renderer3D& r3d)
		{
			mesh = r3d.mesh;
			material = r3d.material;
			renderingOrder = r3d.renderingOrder;
			isEnabled = r3d.isEnabled;
			castShadows = r3d.castShadows;
			objectVisibilityMask = r3d.objectVisibilityMask;
			shadowVisibilityMask = r3d.shadowVisibilityMask;
			skeletalAnimatorEntityUUID = r3d.skeletalAnimatorEntityUUID;
		}

		/**
		 * Used for transparency. From 0 to 11 [-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5].
		 */
		int renderingOrder = 5;

		bool isEnabled = true;
		bool castShadows = true;

		uint8_t objectVisibilityMask = -1;
		uint8_t shadowVisibilityMask = -1;

		UUID skeletalAnimatorEntityUUID = UUID(0, 0);

		/**
		 * Index of an entity in EntityBuffer that will be used to index on GPU.
		 */
		uint32_t entityIndex = -1;

		/**
		 * Recalculated every frame in ProcessEntities.
		 */
		AABB aabb;

		/**
		 * Per-entity GPU buffer containing pre-computed skinned vertex data (position, normal, tangent).
		 * Created lazily for skinned meshes with a SkeletalAnimator. Written by GPUSkinningPass.
		 */
		std::shared_ptr<Buffer> skinnedVertexBuffer;

		/**
		 * Per-entity BLAS built from skinnedVertexBuffer. Rebuilt every frame in GPUSkinningPass.
		 * Used instead of mesh->GetBLAS() for TLAS instance construction.
		 */
		std::shared_ptr<AccelerationStructure> skinnedBLAS;

		~Renderer3D();
	};

}
