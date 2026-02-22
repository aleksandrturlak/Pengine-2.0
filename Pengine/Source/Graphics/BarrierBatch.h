#pragma once

#include "../Core/Core.h"

namespace Pengine
{

	class Texture;

	enum class PipelineStage : uint32_t
	{
		None            = 0,
		DrawIndirect    = 1 << 0,
		VertexInput     = 1 << 1,
		VertexShader    = 1 << 2,
		FragmentShader  = 1 << 3,
		ColorAttachment = 1 << 4,
		EarlyDepth      = 1 << 5,
		LateDepth       = 1 << 6,
		ComputeShader   = 1 << 7,
		Transfer        = 1 << 8,
		Host            = 1 << 9,
		AllGraphics     = 1 << 10,
		AllCommands     = 1 << 11,
	};

	inline PipelineStage operator|(PipelineStage a, PipelineStage b)
	{
		return static_cast<PipelineStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	inline PipelineStage operator&(PipelineStage a, PipelineStage b)
	{
		return static_cast<PipelineStage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}

	enum class Access : uint32_t
	{
		None                 = 0,
		IndirectCommandRead  = 1 << 0,
		VertexAttributeRead  = 1 << 1,
		UniformRead          = 1 << 2,
		ShaderRead           = 1 << 3,
		ShaderWrite          = 1 << 4,
		ColorAttachmentRead  = 1 << 5,
		ColorAttachmentWrite = 1 << 6,
		DepthAttachmentRead  = 1 << 7,
		DepthAttachmentWrite = 1 << 8,
		TransferRead         = 1 << 9,
		TransferWrite        = 1 << 10,
		HostRead             = 1 << 11,
		HostWrite            = 1 << 12,
		MemoryRead           = 1 << 13,
		MemoryWrite          = 1 << 14,
	};

	inline Access operator|(Access a, Access b)
	{
		return static_cast<Access>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	inline Access operator&(Access a, Access b)
	{
		return static_cast<Access>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}

	enum class ImageLayout
	{
		Undefined,
		General,
		ColorAttachment,
		DepthAttachment,
		ShaderReadOnly,
		TransferSrc,
		TransferDst,
		Present,
	};

	struct MemoryBarrierDesc
	{
		Access srcAccess = Access::None;
		Access dstAccess = Access::None;
	};

	struct BufferBarrierDesc
	{
		NativeHandle buffer   = NativeHandle::Invalid();
		size_t       offset   = 0;
		size_t       size     = ~0ull;
		Access       srcAccess = Access::None;
		Access       dstAccess = Access::None;
	};

	struct ImageBarrierDesc
	{
		std::shared_ptr<Texture> texture;
		ImageLayout  oldLayout   = ImageLayout::Undefined;
		ImageLayout  newLayout   = ImageLayout::General;
		Access       srcAccess   = Access::None;
		Access       dstAccess   = Access::None;
		uint32_t     baseMip     = 0;
		uint32_t     mipCount    = ~0u;
		uint32_t     baseLayer   = 0;
		uint32_t     layerCount  = ~0u;
	};

	struct BarrierBatch
	{
		PipelineStage srcStage = PipelineStage::None;
		PipelineStage dstStage = PipelineStage::None;

		std::vector<MemoryBarrierDesc> memory;
		std::vector<BufferBarrierDesc> buffers;
		std::vector<ImageBarrierDesc>  images;

		BarrierBatch& Stages(PipelineStage src, PipelineStage dst)
		{
			srcStage = src;
			dstStage = dst;
			return *this;
		}

		BarrierBatch& Memory(Access src, Access dst)
		{
			memory.push_back({ src, dst });
			return *this;
		}

		BarrierBatch& Buffer(NativeHandle buffer, Access src, Access dst,
		                     size_t offset = 0, size_t size = ~0ull)
		{
			buffers.push_back({ buffer, offset, size, src, dst });
			return *this;
		}

		BarrierBatch& Image(std::shared_ptr<Texture> texture,
		                    ImageLayout oldLayout, ImageLayout newLayout,
		                    Access src, Access dst,
		                    uint32_t baseMip = 0, uint32_t mipCount = ~0u,
		                    uint32_t baseLayer = 0, uint32_t layerCount = ~0u)
		{
			images.push_back({ std::move(texture), oldLayout, newLayout, src, dst, baseMip, mipCount, baseLayer, layerCount });
			return *this;
		}
	};

}
