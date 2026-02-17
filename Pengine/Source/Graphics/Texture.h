#pragma once

#include "../Core/Core.h"
#include "../Core/Asset.h"

#include "Format.h"

namespace Pengine
{

	class PENGINE_API Texture : public Asset
	{
	public:

		enum class Layout
		{
			UNDEFINED = 0,
			GENERAL = 1,
			COLOR_ATTACHMENT_OPTIMAL = 2,
			DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
			DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
			SHADER_READ_ONLY_OPTIMAL = 5,
			TRANSFER_SRC_OPTIMAL = 6,
			TRANSFER_DST_OPTIMAL = 7,
			PREINITIALIZED = 8,
			DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1000117000,
			DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1000117001,
			DEPTH_ATTACHMENT_OPTIMAL = 1000241000,
			DEPTH_READ_ONLY_OPTIMAL = 1000241001,
			STENCIL_ATTACHMENT_OPTIMAL = 1000241002,
			STENCIL_READ_ONLY_OPTIMAL = 1000241003,
			READ_ONLY_OPTIMAL = 1000314000,
			ATTACHMENT_OPTIMAL = 1000314001,
			RENDERING_LOCAL_READ = 1000232000,
			PRESENT_SRC_KHR = 1000001002,
			VIDEO_DECODE_DST_KHR = 1000024000,
			VIDEO_DECODE_SRC_KHR = 1000024001,
			VIDEO_DECODE_DPB_KHR = 1000024002,
			SHARED_PRESENT_KHR = 1000111000,
			FRAGMENT_DENSITY_MAP_OPTIMAL_EXT = 1000218000,
			FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR = 1000164003,
			VIDEO_ENCODE_DST_KHR = 1000299000,
			VIDEO_ENCODE_SRC_KHR = 1000299001,
			VIDEO_ENCODE_DPB_KHR = 1000299002,
			ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT = 1000339000,
			VIDEO_ENCODE_QUANTIZATION_MAP_KHR = 1000553000,
			DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR = DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
			DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR = DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
			SHADING_RATE_OPTIMAL_NV = FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
			RENDERING_LOCAL_READ_KHR = RENDERING_LOCAL_READ,
			DEPTH_ATTACHMENT_OPTIMAL_KHR = DEPTH_ATTACHMENT_OPTIMAL,
			DEPTH_READ_ONLY_OPTIMAL_KHR = DEPTH_READ_ONLY_OPTIMAL,
			STENCIL_ATTACHMENT_OPTIMAL_KHR = STENCIL_ATTACHMENT_OPTIMAL,
			STENCIL_READ_ONLY_OPTIMAL_KHR = STENCIL_READ_ONLY_OPTIMAL,
			READ_ONLY_OPTIMAL_KHR = READ_ONLY_OPTIMAL,
			ATTACHMENT_OPTIMAL_KHR = ATTACHMENT_OPTIMAL,
			MAX_ENUM = 0x7FFFFFFF
		};

		enum class AspectMask
		{
			COLOR,
			DEPTH
		};

		enum class Usage
		{
			SAMPLED,
			TRANSFER_DST,
			TRANSFER_SRC,
			DEPTH_STENCIL_ATTACHMENT,
			COLOR_ATTACHMENT,
			STORAGE
		};

		struct SamplerCreateInfo
		{
			enum class Filter
			{
				NEAREST = 0,
				LINEAR = 1
			};

			enum class AddressMode
			{
				REPEAT = 0,
				MIRRORED_REPEAT = 1,
				CLAMP_TO_EDGE = 2,
				CLAMP_TO_BORDER = 3,
				MIRROR_CLAMP_TO_EDGE = 4
			};

			enum class MipmapMode
			{
				MODE_NEAREST = 0,
				MODE_LINEAR = 1
			};

			enum class BorderColor
			{
				FLOAT_TRANSPARENT_BLACK = 0,
				INT_TRANSPARENT_BLACK = 1,
				FLOAT_OPAQUE_BLACK = 2,
				INT_OPAQUE_BLACK = 3,
				FLOAT_OPAQUE_WHITE = 4,
				INT_OPAQUE_WHITE = 5
			};

			float maxAnisotropy = 16.0f;
			Filter filter = Filter::LINEAR;
			AddressMode addressMode = AddressMode::REPEAT;
			MipmapMode mipmapMode = MipmapMode::MODE_LINEAR;
			BorderColor borderColor = BorderColor::FLOAT_OPAQUE_BLACK;
		};

		struct Region
		{
			glm::ivec3 srcOffset = { 0, 0, 0 };
			glm::ivec3 dstOffset = { 0, 0, 0 };
			glm::uvec3 extent = { 0, 0, 1 };
		};

		struct Meta
		{
			std::filesystem::path filepath;
			UUID uuid;
			bool createMipMaps = true;
			bool srgb = false;
		};

		struct CreateInfo
		{
			SamplerCreateInfo samplerCreateInfo{};
			glm::ivec2 size = { 0, 0 };
			void* data = nullptr;
			uint32_t mipLevels = 1;
			uint32_t layerCount = 1;
			uint32_t instanceSize = 0;
			std::string name;
			std::filesystem::path filepath;
			Format format;
			AspectMask aspectMask;
			std::vector<Usage> usage;
			bool isCubeMap = false;
			bool isMultiBuffered = false;
			
			MemoryType memoryType = MemoryType::GPU;

			Meta meta;
		};

		struct LoadInfo
		{
			std::filesystem::path filepath;
			bool createMipMaps = true;
			bool srgb = true;
		};

		struct SubresourceLayout
		{
			uint64_t offset;
			uint64_t size;
			uint64_t rowPitch;
			uint64_t arrayPitch;
			uint64_t depthPitch;
		};

		static std::shared_ptr<Texture> Create(const CreateInfo& createInfo);

		static std::shared_ptr<Texture> Load(const std::filesystem::path& filepath, bool flip, const Meta& meta);
		
		explicit Texture(const CreateInfo& createInfo);
		virtual ~Texture() = default;
		Texture(const Texture&) = delete;
		Texture(Texture&&) = delete;
		Texture& operator=(const Texture&) = delete;
		Texture& operator=(Texture&&) = delete;

		[[nodiscard]] virtual void* GetId(const uint32_t frameIndex = Vk::frameInFlightIndex) const { return nullptr;  }

		[[nodiscard]] virtual void* GetData() const = 0;

		[[nodiscard]] virtual SubresourceLayout GetSubresourceLayout() const = 0;

		virtual void GenerateMipMaps(void* frame = nullptr) = 0;

		virtual void Copy(std::shared_ptr<Texture> src, const Region& region, void* frame = nullptr) = 0;

		virtual void Transition(Layout layout, void* frame = nullptr) = 0;

		[[nodiscard]] glm::ivec2 GetSize() const { return m_Size; }

		[[nodiscard]] uint32_t GetInstanceSize() const { return m_InstanceSize; }

		[[nodiscard]] Format GetFormat() const { return m_Format; }

		[[nodiscard]] AspectMask GetAspectMask() const { return m_AspectMask; }

		[[nodiscard]] uint32_t GetMipLevels() const { return m_MipLevels; }

		[[nodiscard]] uint32_t GetLayerCount() const { return m_LayerCount; }

		[[nodiscard]] MemoryType GetMemoryType() const { return m_MemoryType; }

		[[nodiscard]] Meta GetMeta() const { return m_Meta; }

		[[nodiscard]] bool IsMultiBuffered() const { return m_IsMultiBuffered; }

		[[nodiscard]] std::shared_ptr<class UniformWriter> GetUniformWriter() const { return m_UniformWriter; }

		[[nodiscard]] int GetBindlessIndex() const { return m_BindlessIndex; }

		void SetBindlessIndex(const int index) { m_BindlessIndex = index; }

	protected:
		glm::ivec2 m_Size = { 0, 0 };

		uint32_t m_MipLevels = 1;
		uint32_t m_LayerCount = 1;
		uint32_t m_InstanceSize = sizeof(uint8_t) * 4;

		Format m_Format{};
		AspectMask m_AspectMask{};
		MemoryType m_MemoryType{};

		int m_BindlessIndex = 0;

		bool m_IsMultiBuffered = false;

		Meta m_Meta{};

		std::shared_ptr<class UniformWriter> m_UniformWriter;
	};

}