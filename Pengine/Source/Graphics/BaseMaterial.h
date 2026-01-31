#pragma once

#include "../Core/Core.h"
#include "../Core/Asset.h"

#include "GraphicsPipeline.h"
#include "ComputePipeline.h"
#include "UniformWriter.h"
#include "UniformLayout.h"

#include "../Utils/Utils.h"

namespace Pengine
{

	class PENGINE_API BaseMaterial final : public Asset
	{
	public:
		struct CreateInfo
		{
			std::vector<GraphicsPipeline::CreateGraphicsInfo> pipelineCreateGraphicsInfos;
			std::vector<ComputePipeline::CreateComputeInfo> pipelineCreateComputeInfos;
		};

		static std::shared_ptr<BaseMaterial> Create(
			const std::string& name,
			const std::filesystem::path& filepath,
			const CreateInfo& createInfo);

		static std::shared_ptr<BaseMaterial> Load(const std::filesystem::path& filepath);

		static void Reload(const std::shared_ptr<BaseMaterial>& baseMaterial);

		BaseMaterial(
			const std::string& name,
			const std::filesystem::path& filepath,
			const CreateInfo& createInfo);
		~BaseMaterial();
		BaseMaterial(const BaseMaterial&) = delete;
		BaseMaterial& operator=(const BaseMaterial&) = delete;

		std::shared_ptr<Pipeline> GetPipeline(const std::string& passName) const;

		std::unordered_map<std::string, std::shared_ptr<Pipeline>> GetPipelinesByPass() const { return m_PipelinesByPass; }

		std::shared_ptr<UniformWriter> GetUniformWriter(const std::string& passName) const;

		std::shared_ptr<Buffer> GetBuffer(const std::string& name) const;

		bool GetUniformDetails(
			const std::string& uniformBufferName,
			const std::string& valueName,
			uint32_t& size,
			uint32_t& offset) const;

		std::optional<ShaderReflection::ReflectVariable> GetUniformValue(
			const std::string& uniformBufferName,
			const std::string& valueName);

		std::shared_ptr<Texture> GetBindlessTexture(const int index) const;

		int BindBindlessTexture(const std::shared_ptr<Texture>& texture);

		void UnBindBindlessTexture(const std::shared_ptr<Texture>& texture);

		std::shared_ptr<Buffer> GetBaseMaterialInfoBuffer() const { return m_BaseMaterialInfoBuffer; }

		template<typename T>
		void WriteToBuffer(
			const std::string& uniformBufferName,
			const std::string& valueName,
			T& value)
		{
			uint32_t size{}, offset{};

			const bool found = GetUniformDetails(uniformBufferName, valueName, size, offset);

			const std::shared_ptr<Buffer> buffer = GetBuffer(uniformBufferName);
			if (buffer && found)
			{
				buffer->WriteToBuffer((void*)&value, size, offset);
			}
			else
			{
				Logger::Warning("Failed to write to buffer: " + uniformBufferName + " | " + valueName + "!");
			}
		}

		template<typename T>
		void WriteToBuffer(
			std::shared_ptr<Buffer> buffer,
			const std::string& uniformBufferName,
			const std::string& valueName,
			T& value)
		{
			uint32_t size{}, offset{};

			const bool found = GetUniformDetails(uniformBufferName, valueName, size, offset);

			if (buffer && found)
			{
				buffer->WriteToBuffer((void*)&value, size, offset);
			}
			else
			{
				Logger::Warning("Failed to write to buffer: " + uniformBufferName + " | " + valueName + "!");
			}
		}

		template<typename T>
		T GetBufferValue(
			std::shared_ptr<Buffer> buffer,
			const std::string& uniformBufferName,
			const std::string& valueName)
		{
			uint32_t size{}, offset{};

			const bool found = GetUniformDetails(uniformBufferName, valueName, size, offset);

			if (buffer && found)
			{
				if (sizeof(T) != size)
				{
					Logger::Warning("Failed to get buffer value: " + uniformBufferName + " | " + valueName + ", size is different!");
				}

				return *(T*)(((uint8_t*)buffer->GetData()) + offset);
			}
			else
			{
				Logger::Warning("Failed to get buffer value: " + uniformBufferName + " | " + valueName + "!");

				return {};
			}
		}

		struct BaseMaterialInfoBuffer
		{
			uint32_t pipelineIds[MAX_PIPELINE_COUNT_PER_MATERIAL];
		};

	private:
		void CreateResources(const CreateInfo& createInfo);

		void CreatePipelineResources(
			const std::string& passName,
			std::shared_ptr<Pipeline> pipeline,
			const Pipeline::UniformInfo& uniformInfo);

		std::unordered_map<std::string, std::shared_ptr<Pipeline>> m_PipelinesByPass;
		std::unordered_map<std::string, std::shared_ptr<UniformWriter>> m_UniformWriterByPass;
		std::unordered_map<std::string, std::shared_ptr<Buffer>> m_BuffersByName;
		
		std::unordered_map<int, std::shared_ptr<class Texture>> m_BindlessTexturesByIndex;

		mutable std::mutex m_UniformCacheMutex;
		// map<BufferName, map<ValueName, <Size, Offset>>>
		mutable std::unordered_map<std::string, std::unordered_map<std::string, std::pair<uint32_t, uint32_t>>> m_UniformsCache;

		std::shared_ptr<Buffer> m_BaseMaterialInfoBuffer;
	};

}
