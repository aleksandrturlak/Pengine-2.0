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
			TRANSFER_SRC,
			TRANSFER_DST,
			ACCELERATION_STRUCTURE_INPUT
		};

		struct CreateInfo
		{
			size_t instanceSize = 0;
			uint32_t instanceCount = 1;
			std::vector<Usage> usages;
			MemoryType memoryType = MemoryType::CPU;
			bool isMultiBuffered = false;
		};

		static std::shared_ptr<Buffer> Create(const CreateInfo& createInfo);

		Buffer(const CreateInfo& createInfo);
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

		[[nodiscard]] MemoryType GetMemoryType() const { return m_CreateInfo.memoryType; }

		[[nodiscard]] bool IsMultiBuffered() const { return m_CreateInfo.isMultiBuffered; }

	protected:
		CreateInfo m_CreateInfo;
	};

}