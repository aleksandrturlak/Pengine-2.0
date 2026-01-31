#pragma once

#include "../Core/Core.h"
#include "../Core/Asset.h"
#include "../Core/Logger.h"

#include "BaseMaterial.h"

#include "../Utils/Utils.h"

namespace Pengine
{

	class PENGINE_API Material final : public Asset
	{
	public:
		struct Option
		{
			bool m_IsEnabled = false;
			std::vector<std::string> m_Active;
			std::vector<std::string> m_Inactive;
		};

		struct CreateInfo
		{
			std::unordered_map<std::string, Pipeline::UniformInfo> uniformInfos;
			std::unordered_map<std::string, Option> optionsByName;
			std::filesystem::path baseMaterial;
		};

		static std::shared_ptr<Material> Create(
			const std::string& name,
			const std::filesystem::path& filepath,
			const CreateInfo& createInfo);

		static std::shared_ptr<Material> Load(const std::filesystem::path& filepath);

		static void Save(const std::shared_ptr<Material>& material, bool useLog = true);

		static void Reload(const std::shared_ptr<Material>& material, bool reloadBaseMaterial = true);

		static std::shared_ptr<Material> Clone(
			const std::string& name,
			const std::filesystem::path& filepath,
			const std::shared_ptr<Material>& material);

		Material(
			const std::string& name,
			const std::filesystem::path& filepath,
			const CreateInfo& createInfo);
		~Material();
		Material(const Material&) = delete;
		Material& operator=(const Material&) = delete;

		std::shared_ptr<BaseMaterial> GetBaseMaterial() const { return m_BaseMaterial; }

		void SetBaseMaterial(std::shared_ptr<BaseMaterial> baseMaterial) { m_BaseMaterial = baseMaterial; }

		std::shared_ptr<UniformWriter> GetUniformWriter(const std::string& passName) const;

		std::shared_ptr<Buffer> GetBuffer(const std::string& name) const;

		bool IsPipelineEnabled(const std::string& passName) const;

		std::unordered_map<std::string, Option> GetOptionsByName() const { return m_OptionsByName; }

		void SetOption(const std::string& name, bool isEnabled);

		template<typename T>
		void WriteToBuffer(
			const std::string& uniformBufferName,
			const std::string& valueName,
			T& value);

		template<typename T>
		T GetBufferValue(
			const std::string& uniformBufferName,
			const std::string& valueName);
		
		std::shared_ptr<Texture> GetBindlessTexture(const int index) const;

		int BindBindlessTexture(const std::shared_ptr<Texture>& texture);

		void UnBindBindlessTexture(const std::shared_ptr<Texture>& texture);

		std::shared_ptr<Buffer> GetMaterialInfoBuffer();
		
	private:
		void CreateResources(const CreateInfo& createInfo);

		void ReloadMaterialInfoBuffer();

		void FlushMaterialInfoBuffer();

		std::shared_ptr<BaseMaterial> m_BaseMaterial;
		std::unordered_map<std::string, std::shared_ptr<UniformWriter>> m_UniformWriterByPass;
		std::unordered_map<std::string, std::shared_ptr<Buffer>> m_BuffersByName;
		std::unordered_map<std::string, Option> m_OptionsByName;
		std::unordered_map<std::string, bool> m_PipelineStates;

		std::unordered_map<int, std::shared_ptr<class Texture>> m_BindlessTexturesByIndex;

		std::shared_ptr<Buffer> m_MaterialInfoBuffer;

		struct MaterialInfo
		{
			uint64_t materialBuffer[MAX_PIPELINE_COUNT_PER_MATERIAL];
			uint64_t baseMaterialInfoBuffer;
			uint64_t pipelineFlags;
		};

		struct MaterialInfoIntermediate
		{
			std::shared_ptr<Buffer> materialBuffer[MAX_PIPELINE_COUNT_PER_MATERIAL];
		} m_MaterialInfoIntermediate{};

		bool m_DirtyOption = false;
	};

	template<typename T>
	inline void Material::WriteToBuffer(
		const std::string& uniformBufferName,
		const std::string& valueName,
		T& value)
	{
		m_BaseMaterial->WriteToBuffer(GetBuffer(uniformBufferName), uniformBufferName, valueName, value);
	}

	template<typename T>
	inline T Material::GetBufferValue(const std::string& uniformBufferName, const std::string& valueName)
	{
		return m_BaseMaterial->GetBufferValue<T>(GetBuffer(uniformBufferName), uniformBufferName, valueName);
	}

}
