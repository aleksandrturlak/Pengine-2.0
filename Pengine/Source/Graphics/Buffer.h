#pragma once

#include "../Core/Core.h"

namespace Pengine
{

	class PENGINE_API Buffer
	{
	public:
		enum class Usage
		{
			UNIFORM_BUFFER,
			VERTEX_BUFFER,
			INDEX_BUFFER,
			STORAGE_BUFFER,
			INDIRECT_BUFFER,
		};

		static std::shared_ptr<Buffer> Create(
			const size_t instanceSize,
			const uint32_t instanceCount,
			const std::vector<Usage>& usages,
			const MemoryType memoryType,
			const bool isMultiBuffered = false);

		Buffer(
			const MemoryType memoryType,
			const bool isMultiBuffered);
		virtual ~Buffer() = default;
		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

		[[nodiscard]] virtual void* GetData() const = 0;

		virtual void WriteToBuffer(
			void* data,
			size_t size,
			size_t offset = 0) = 0;

		virtual void Copy(
			const std::shared_ptr<Buffer>& buffer,
			size_t dstOffset) = 0;

		virtual void Flush() = 0;

		virtual void ClearWrites() = 0;

		[[nodiscard]] virtual NativeHandle GetNativeHandle() const = 0;

		[[nodiscard]] virtual size_t GetSize() const = 0;

		[[nodiscard]] virtual uint32_t GetInstanceCount() const = 0;

		[[nodiscard]] virtual size_t GetInstanceSize() const = 0;

		[[nodiscard]] virtual NativeHandle GetDeviceAddress() const = 0;

		[[nodiscard]] MemoryType GetMemoryType() const { return m_MemoryType; }

		[[nodiscard]] bool IsMultiBuffered() const { return m_IsMultiBuffered; }

	protected:
		MemoryType m_MemoryType = MemoryType::CPU;

		bool m_IsMultiBuffered = false;
	};

}