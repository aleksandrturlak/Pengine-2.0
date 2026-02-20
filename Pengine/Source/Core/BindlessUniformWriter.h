#pragma once

#include "Core.h"
#include "BoundingBox.h"

#include "../Graphics/BaseMaterial.h"

#include <stack>
#include <vector>

namespace Pengine
{

	class Buffer;
	class Texture;
	class UniformWriter;
	class Material;
	class BaseMaterial;
	class Mesh;

	class PENGINE_API BindlessUniformWriter
	{
	public:
		static BindlessUniformWriter& GetInstance();

		BindlessUniformWriter() = default;
		~BindlessUniformWriter() = default;

		BindlessUniformWriter(const BindlessUniformWriter&) = delete;
		BindlessUniformWriter& operator=(const BindlessUniformWriter&) = delete;

		std::shared_ptr<class UniformWriter> GetBindlessUniformWriter() const { return m_BindlessUniformWriter; }

		void Initialize();
		
		void ShutDown();

		int BindTexture(const std::shared_ptr<Texture>& texture);

		void UnBindTexture(const std::shared_ptr<Texture>& texture);

		std::shared_ptr<Texture> GetBindlessTexture(const int index);

		void CreateSceneResources(
			std::shared_ptr<UniformWriter>& uniformWriter,
			std::shared_ptr<Buffer>& buffer);

		enum class EntityFlagBits : uint32_t
		{
			VALID = 1 << 0,
			SKINNED = 1 << 1
		};

		struct EntityInfo
		{
			glm::mat4 transform;
			AABB aabb;
			uint64_t materialInfoBuffer;
			uint64_t meshInfoBuffer;
			uint64_t boneBuffer;
			uint32_t flags; // valid, skinned, etc.
		};

	private:
		class SlotManager
		{
		private:
			std::stack<int> m_FreeSlots;
			std::vector<bool> m_InUse;
		public:
			void Initialize(int slotCount)
			{
				m_InUse.resize(slotCount, false);
				for (int i = slotCount - 1; i >= 0; i--)
				{
					m_FreeSlots.push(i);
				}
			}
				
			int TakeSlot()
			{
				if (m_FreeSlots.empty()) return 0;
				
				int slot = m_FreeSlots.top();
				m_FreeSlots.pop();
				m_InUse[slot] = true;
				return slot;
			}
			
			void FreeSlot(int index)
			{
				if (index < 0 || index >= m_InUse.size()) return;
				if (!m_InUse[index]) return;
				
				m_InUse[index] = false;
				m_FreeSlots.push(index);
			}
			
			bool IsSlotFree(int index) const
			{
				return !m_InUse[index];
			}
			
			int FreeCount() const
			{
				return m_FreeSlots.size();
			}
		};

		SlotManager m_TextureSlotManager;

		std::unordered_map<int, std::weak_ptr<Texture>> m_TexturesByIndex;

		std::shared_ptr<UniformWriter> m_BindlessUniformWriter;
		std::shared_ptr<BaseMaterial> m_BaseMaterial;

		size_t m_BaseMaterialSize = 0;
	};

}
