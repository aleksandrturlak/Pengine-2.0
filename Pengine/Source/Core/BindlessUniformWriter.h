#pragma once

#include "Core.h"

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

		int BindMaterial(const std::shared_ptr<Material>& material);

		void UnBindMaterial(const std::shared_ptr<Material>& material);

		std::shared_ptr<Material> GetBindlessMaterial(const int index);

		std::shared_ptr<BaseMaterial> GetBaseMaterial() const { return m_BaseMaterial; }

		std::shared_ptr<Buffer> GetBindlessMaterialBuffer() const { return m_BindlessMaterialBuffer; }

		void* GetBindlessMaterialBufferData(const int index) const { return (void*)((uint8_t*)m_BindlessMaterialBuffer->GetData() + m_BaseMaterialSize * index); }

		void Flush();

		template<typename T>
		inline void WriteToBuffer(
			const int index,
			const std::string& valueName,
			T& value)
		{
			assert(m_BindlessMaterialBuffer);

			uint32_t size, offset;
			if (m_BaseMaterial->GetUniformDetails("Material", valueName, size, offset))
			{
				m_BindlessMaterialBuffer->WriteToBuffer((void*)&value, size, index * m_BaseMaterialSize + offset);
			}
			else
			{
				Logger::Warning("Failed to write to bindless buffer: " + valueName + "!");
			}
		}

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
		SlotManager m_MaterialSlotManager;

		std::unordered_map<int, std::weak_ptr<Texture>> m_TexturesByIndex;
		std::unordered_map<int, std::weak_ptr<Material>> m_MaterialsByIndex;

		std::shared_ptr<UniformWriter> m_BindlessUniformWriter;
		std::shared_ptr<BaseMaterial> m_BaseMaterial;
		std::shared_ptr<Buffer> m_BindlessMaterialBuffer;

		size_t m_BaseMaterialSize = 0;
	};

}
