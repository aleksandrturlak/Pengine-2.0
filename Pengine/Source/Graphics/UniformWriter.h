#pragma once

#include "../Core/Core.h"

#include "Buffer.h"
#include "Texture.h"
#include "AccelerationStructure.h"
#include "UniformLayout.h"

namespace Pengine
{

	class PENGINE_API UniformWriter
	{
	public:
		struct TextureInfo
		{
			std::shared_ptr<Texture> texture;
			uint32_t baseMipLevel = 0;
		};
	
		static std::shared_ptr<UniformWriter> Create(
			std::shared_ptr<UniformLayout> uniformLayout,
			bool isMultiBuffered = true);

		explicit UniformWriter(
			std::shared_ptr<UniformLayout> uniformLayout,
			bool isMultiBuffered);
		virtual ~UniformWriter();
		UniformWriter(const UniformWriter&) = delete;
		UniformWriter& operator=(const UniformWriter&) = delete;

		void WriteAccelerationStructureToFrame(
			uint32_t location,
			const std::shared_ptr<AccelerationStructure>& accelerationStructure,
			uint32_t frameIndex = Vk::frameInFlightIndex);

		void WriteAccelerationStructureToAllFrames(
			uint32_t location,
			const std::shared_ptr<AccelerationStructure>& accelerationStructure);

		void WriteBufferToFrame(
			uint32_t location,
			const std::shared_ptr<Buffer>& buffer,
			size_t size = -1,
			size_t offset = 0,
			uint32_t frameIndex = Vk::frameInFlightIndex);

		void WriteBufferToAllFrames(
			uint32_t location,
			const std::shared_ptr<Buffer>& buffer,
			size_t size = -1,
			size_t offset = 0);

		void WriteTextureToFrame(
			uint32_t location,
			const TextureInfo& textureInfo,
			uint32_t dstArrayElement = 0,
			uint32_t srcFrameIndex = Vk::frameInFlightIndex,
			uint32_t dstFrameIndex = Vk::frameInFlightIndex);

		void WriteTexturesToFrame(
			uint32_t location,
			const std::vector<TextureInfo>& textureInfos,
			uint32_t dstArrayElement = 0,
			uint32_t srcFrameIndex = Vk::frameInFlightIndex,
			uint32_t dstFrameIndex = Vk::frameInFlightIndex);

		void WriteTextureToAllFrames(
			uint32_t location,
			const TextureInfo& textureInfo,
			uint32_t dstArrayElement = 0);

		void WriteTexturesToAllFrames(
			uint32_t location,
			const std::vector<TextureInfo>& textureInfos,
			uint32_t dstArrayElement = 0);

		void WriteAccelerationStructureToFrame(
			const std::string& name,
			const std::shared_ptr<AccelerationStructure>& accelerationStructure,
			uint32_t frameIndex = Vk::frameInFlightIndex);

		void WriteAccelerationStructureToAllFrames(
			const std::string& name,
			const std::shared_ptr<AccelerationStructure>& accelerationStructure);

		void WriteBufferToFrame(
			const std::string& name,
			const std::shared_ptr<Buffer>& buffer,
			size_t size = -1,
			size_t offset = 0,
			uint32_t frameIndex = Vk::frameInFlightIndex);

		void WriteBufferToAllFrames(
			const std::string& name,
			const std::shared_ptr<Buffer>& buffer,
			size_t size = -1,
			size_t offset = 0);

		void WriteTextureToFrame(
			const std::string& name,
			const TextureInfo& textureInfo,
			uint32_t dstArrayElement = 0,
			uint32_t srcFrameIndex = Vk::frameInFlightIndex,
			uint32_t dstFrameIndex = Vk::frameInFlightIndex);

		void WriteTexturesToFrame(
			const std::string& name,
			const std::vector<TextureInfo>& textureInfos,
			uint32_t dstArrayElement = 0,
			uint32_t srcFrameIndex = Vk::frameInFlightIndex,
			uint32_t dstFrameIndex = Vk::frameInFlightIndex);

		void WriteTextureToAllFrames(
			const std::string& name,
			const TextureInfo& textureInfo,
			uint32_t dstArrayElement = 0);

		void WriteTexturesToAllFrames(
			const std::string& name,
			const std::vector<TextureInfo>& textureInfos,
			uint32_t dstArrayElement = 0);

		void WriteTextureToFrame(
			const std::string& name,
			const std::shared_ptr<Texture>& texture,
			uint32_t dstArrayElement = 0,
			uint32_t srcFrameIndex = Vk::frameInFlightIndex,
			uint32_t dstFrameIndex = Vk::frameInFlightIndex);

		void WriteTextureToAllFrames(
			const std::string& name,
			const std::shared_ptr<Texture>& texture,
			uint32_t dstArrayElement = 0);

		void DeleteBuffer(const std::string& name);

		void DeleteTexture(const std::string& name);

		virtual void Flush() = 0;
		virtual NativeHandle GetNativeHandle() const = 0;

		const std::unordered_map<std::string, std::vector<std::shared_ptr<Buffer>>>& GetBuffersByName() const { return m_BuffersByName; }

		std::vector<std::shared_ptr<Buffer>> GetBuffer(const std::string& name);

		const std::unordered_map<std::string, std::vector<TextureInfo>>& GetTexturesByName() const { return m_TextureInfosByName; }

		std::vector<TextureInfo> GetTextureInfo(const std::string& name);
		
		std::shared_ptr<UniformLayout> GetUniformLayout() const { return m_UniformLayout; }

		[[nodiscard]] bool IsMultiBuffered() const { return m_IsMultiBuffered; }

	protected:
		std::shared_ptr<UniformLayout> m_UniformLayout;

		std::unordered_map<std::string, std::vector<TextureInfo>> m_TextureInfosByName;
		std::unordered_map<std::string, std::vector<std::shared_ptr<Buffer>>> m_BuffersByName;
		std::unordered_map<std::string, std::vector<std::shared_ptr<AccelerationStructure>>> m_AccelerationStructuresByName;

		std::unordered_map<uint32_t, std::string> m_BufferNameByLocation;
		std::unordered_map<uint32_t, std::string> m_TextureNameByLocation;
		std::unordered_map<uint32_t, std::string> m_AccelerationStructureNameByLocation;

		struct BufferWrite
		{
			ShaderReflection::ReflectDescriptorSetBinding binding;
			std::vector<std::shared_ptr<Buffer>> buffers;
			size_t offset = 0;
			size_t size = -1;
		};

		struct TextureWrite
		{
			ShaderReflection::ReflectDescriptorSetBinding binding;
			std::vector<TextureInfo> textureInfos;
			uint32_t dstArrayElement = 0;
			uint32_t frameIndex = 0;
		};

		struct AccelerationStructureWrite
		{
			ShaderReflection::ReflectDescriptorSetBinding binding;
			std::vector<std::shared_ptr<AccelerationStructure>> accelerationStructures;
		};

		struct Write
		{
			std::unordered_map<uint32_t, BufferWrite> bufferWritesByLocation;
			std::unordered_map<uint32_t, std::vector<TextureWrite>> textureWritesByLocation;
			std::unordered_map<uint32_t, AccelerationStructureWrite> accelerationStructureWritesByLocation;
		};

		std::vector<Write> m_Writes;

		std::mutex mutex;

		bool m_IsMultiBuffered = false;
	};

}
