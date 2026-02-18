#include "Material.h"

#include "../Core/Profiler.h"
#include "../Core/Serializer.h"
#include "../Core/TextureManager.h"
#include "../Core/MaterialManager.h"
#include "../Core/AsyncAssetLoader.h"
#include "../Core/BindlessUniformWriter.h"
#include "../Core/RenderPassManager.h"
#include "../EventSystem/EventSystem.h"
#include "../EventSystem/NextFrameEvent.h"
#include "../Graphics/Pass.h"
#include "../Graphics/Device.h"

using namespace Pengine;

std::shared_ptr<Material> Material::Create(const std::string& name, const std::filesystem::path& filepath,
	const CreateInfo& createInfo)
{
	PROFILER_SCOPE(__FUNCTION__);

	return std::make_shared<Material>(name, filepath, createInfo);
}

std::shared_ptr<Material> Material::Load(const std::filesystem::path& filepath)
{
	PROFILER_SCOPE(__FUNCTION__);

	return Create(Utils::GetFilename(filepath), filepath, Serializer::LoadMaterial(filepath));
}

void Material::Save(const std::shared_ptr<Material>& material, bool useLog)
{
	PROFILER_SCOPE(__FUNCTION__);

	Serializer::SerializeMaterial(material, useLog);
}

void Material::Reload(const std::shared_ptr<Material>& material, bool reloadBaseMaterial)
{
	PROFILER_SCOPE(__FUNCTION__);

	if (reloadBaseMaterial)
	{
		BaseMaterial::Reload(material->m_BaseMaterial);
	}

	auto callback = [material]()
	{
		material->m_UniformWriterByPass.clear();
		material->m_OptionsByName.clear();
		material->m_PipelineStates.clear();
		material->m_BuffersByName.clear();

		std::vector<std::shared_ptr<Texture>> textures;
		textures.reserve(material->m_BindlessTexturesByIndex.size());
		
		for (const auto& [index, texture] : material->m_BindlessTexturesByIndex)
		{
			textures.emplace_back(texture);
		}
		material->m_BindlessTexturesByIndex.clear();

		for (auto& texture : textures)
		{
			TextureManager::GetInstance().Delete(texture);
		}

		try
		{
			material->CreateResources(Serializer::LoadMaterial(material->GetFilepath()));
		}
		catch (const std::exception&)
		{

		}
	};

	std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, material.get());
	EventSystem::GetInstance().SendEvent(event);
}

std::shared_ptr<Material> Material::Clone(
	const std::string& name,
	const std::filesystem::path& filepath,
	const std::shared_ptr<Material>& material)
{
	PROFILER_SCOPE(__FUNCTION__);

	CreateInfo createInfo{};
	createInfo.baseMaterial = material->GetBaseMaterial()->GetFilepath();
	createInfo.optionsByName = material->GetOptionsByName();

	for (const auto& [passName, pipeline] : material->GetBaseMaterial()->GetPipelinesByPass())
	{
		std::vector<std::shared_ptr<UniformLayout>> uniformLayouts;

		{
			std::optional<uint32_t> descriptorSetIndex = pipeline->GetDescriptorSetIndexByType(Pipeline::DescriptorSetIndexType::MATERIAL, passName);
			if (descriptorSetIndex)
			{
				uniformLayouts.emplace_back(pipeline->GetUniformLayout(*descriptorSetIndex));
			}
		}

		{
			std::optional<uint32_t> descriptorSetIndex = pipeline->GetDescriptorSetIndexByType(Pipeline::DescriptorSetIndexType::BUFFER_DEVICE_ADDRESS, passName);
			if (descriptorSetIndex)
			{
				uniformLayouts.emplace_back(pipeline->GetUniformLayout(*descriptorSetIndex));
			}
		}

		for (const auto& uniformLayout : uniformLayouts)
		{
			for (const auto& binding : uniformLayout->GetBindings())
			{
				if (binding.type == ShaderReflection::Type::COMBINED_IMAGE_SAMPLER)
				{
					createInfo.uniformInfos[passName].texturesByName[binding.name] = material->GetUniformWriter(passName)->GetTextureInfo(binding.name).back().texture->GetFilepath().string();
				}
				else if (binding.type == ShaderReflection::Type::UNIFORM_BUFFER || binding.type == ShaderReflection::Type::STORAGE_BUFFER)
				{
					const std::shared_ptr<Buffer> buffer = material->GetBuffer(binding.name);
					void* data = buffer->GetData();

					auto& uniformBufferInfo = createInfo.uniformInfos[passName].uniformBuffersByName[binding.name];

					std::function<void(const ShaderReflection::ReflectVariable&, std::string)> copyValue = [
						data,
						&material,
						&copyValue,
						&uniformBufferInfo]
					(const ShaderReflection::ReflectVariable& value, std::string parentName)
					{
						parentName += value.name;

						if (value.type == ShaderReflection::ReflectVariable::Type::VEC2)
						{
							uniformBufferInfo.vec2ValuesByName.emplace(parentName, Utils::GetValue<glm::vec2>(data, value.offset));
						}
						else if (value.type == ShaderReflection::ReflectVariable::Type::VEC3)
						{
							uniformBufferInfo.vec3ValuesByName.emplace(parentName, Utils::GetValue<glm::vec3>(data, value.offset));
						}
						else if (value.type == ShaderReflection::ReflectVariable::Type::VEC4)
						{
							uniformBufferInfo.vec4ValuesByName.emplace(parentName, Utils::GetValue<glm::vec4>(data, value.offset));
						}
						else if (value.type == ShaderReflection::ReflectVariable::Type::FLOAT)
						{
							uniformBufferInfo.floatValuesByName.emplace(parentName, Utils::GetValue<float>(data, value.offset));
						}
						else if (value.type == ShaderReflection::ReflectVariable::Type::INT)
						{
							uniformBufferInfo.intValuesByName.emplace(parentName, Utils::GetValue<int>(data, value.offset));
						}
						else if (value.type == ShaderReflection::ReflectVariable::Type::STRUCT)
						{
							for (const auto& memberValue : value.variables)
							{
								copyValue(memberValue, parentName + ".");
							}
						}
						else if (value.type == ShaderReflection::ReflectVariable::Type::TEXTURE)
						{
							const int index = Utils::GetValue<int>(data, value.offset);
							uniformBufferInfo.texturesByName.emplace(parentName, material->GetBindlessTexture(index)->GetFilepath());
						}
					};

					for (const auto& value : binding.buffer->variables)
					{
						copyValue(value, "");
					}
				}
			}
		}
	}

	return Create(name, filepath, createInfo);
}

Material::Material(
	const std::string& name,
	const std::filesystem::path& filepath,
	const CreateInfo& createInfo)
	: Asset(name, filepath)
{
	CreateResources(createInfo);
}

Material::~Material()
{
	std::vector<std::shared_ptr<Texture>> textures;
	textures.reserve(m_BindlessTexturesByIndex.size());
	
	for (const auto& [index, texture] : m_BindlessTexturesByIndex)
	{
		textures.emplace_back(texture);
	}
	m_BindlessTexturesByIndex.clear();

	for (auto& texture : textures)
	{
		TextureManager::GetInstance().Delete(texture);
	}

	MaterialManager::GetInstance().DeleteBaseMaterial(m_BaseMaterial);
}

std::shared_ptr<UniformWriter> Material::GetUniformWriter(const std::string& passName) const
{
	return Utils::Find(passName, m_UniformWriterByPass);
}

std::shared_ptr<Buffer> Material::GetBuffer(const std::string& name) const
{
	return Utils::Find(name, m_BuffersByName);
}

bool Material::IsPipelineEnabled(const std::string& passName) const
{
	auto pipelineState = m_PipelineStates.find(passName);
	if (pipelineState != m_PipelineStates.end())
	{
		return pipelineState->second;
	}

	return true;
}

void Material::SetOption(const std::string& name, bool isEnabled)
{
	auto foundOption = m_OptionsByName.find(name);
	if (foundOption == m_OptionsByName.end())
	{
		return;
	}

	foundOption->second.m_IsEnabled = isEnabled;

	for (const std::string& active : foundOption->second.m_Active)
	{
		m_PipelineStates[active] = isEnabled;
	}

	for (const std::string& inactive : foundOption->second.m_Inactive)
	{
		m_PipelineStates[inactive] = !isEnabled;
	}

	m_DirtyOption = true;
}

std::shared_ptr<Texture> Material::GetBindlessTexture(const int index) const
{
	auto bindlessTextureByIndex = m_BindlessTexturesByIndex.find(index);
	if (bindlessTextureByIndex != m_BindlessTexturesByIndex.end())
	{
		return bindlessTextureByIndex->second;
	}

	return nullptr;
}

int Material::BindBindlessTexture(const std::shared_ptr<Texture>& texture)
{
    const int index = BindlessUniformWriter::GetInstance().BindTexture(texture);
	m_BindlessTexturesByIndex[index] = texture;
	return index;
}

void Material::UnBindBindlessTexture(const std::shared_ptr<Texture>& texture)
{
	m_BindlessTexturesByIndex.erase(texture->GetBindlessIndex());
	BindlessUniformWriter::GetInstance().UnBindTexture(texture);
}

std::shared_ptr<Buffer> Material::GetMaterialInfoBuffer()
{
    ReloadMaterialInfoBuffer();
	FlushMaterialInfoBuffer();
	return m_MaterialInfoBuffer;
}

void Material::CreateResources(const CreateInfo& createInfo)
{
	m_BaseMaterial = AsyncAssetLoader::GetInstance().SyncLoadBaseMaterial(createInfo.baseMaterial);
	m_OptionsByName = createInfo.optionsByName;
	for (const auto& [name, option] : m_OptionsByName)
	{
		SetOption(name, option.m_IsEnabled);
	}

	for (const auto& [passName, pipeline] : m_BaseMaterial->GetPipelinesByPass())
	{
		if (auto materialIndex = pipeline->GetDescriptorSetIndexByType(Pipeline::DescriptorSetIndexType::MATERIAL, passName))
		{
			const std::shared_ptr<UniformLayout> uniformLayout = pipeline->GetUniformLayout(*materialIndex);
			const std::shared_ptr<UniformWriter> uniformWriter = UniformWriter::Create(uniformLayout);
			m_UniformWriterByPass[passName] = uniformWriter;

			for (const auto& binding : uniformLayout->GetBindings())
			{
				if (binding.buffer)
				{
					Buffer::Usage usage{};
					if (binding.type == ShaderReflection::Type::UNIFORM_BUFFER)
					{
						usage = Buffer::Usage::UNIFORM_BUFFER;
					}
					else if (binding.type == ShaderReflection::Type::STORAGE_BUFFER)
					{
						usage = Buffer::Usage::STORAGE_BUFFER;
					}
					
					Buffer::CreateInfo createInfo{};
					createInfo.instanceSize = binding.buffer->size;
					createInfo.instanceCount = 1;
					createInfo.usages = { usage };
					createInfo.memoryType = MemoryType::CPU;
					createInfo.isMultiBuffered = true;
					const std::shared_ptr<Buffer> buffer = Buffer::Create(createInfo);
					
					m_BuffersByName[binding.buffer->name] = buffer;
					uniformWriter->WriteBufferToAllFrames(binding.buffer->name, buffer);
					uniformWriter->Flush();
				}
			}
		}

		if (auto materialIndex = pipeline->GetDescriptorSetIndexByType(Pipeline::DescriptorSetIndexType::BUFFER_DEVICE_ADDRESS, passName))
		{
			std::shared_ptr<Pass> pass = RenderPassManager::GetInstance().GetPass(passName);

			const std::shared_ptr<UniformLayout> uniformLayout = pipeline->GetUniformLayout(*materialIndex);
			for (const auto& binding : uniformLayout->GetBindings())
			{
				if (binding.buffer)
				{
					Buffer::CreateInfo createInfo{};
					createInfo.instanceSize = binding.buffer->size;
					createInfo.instanceCount = 1;
					createInfo.usages = { Buffer::Usage::STORAGE_BUFFER };
					createInfo.memoryType = MemoryType::CPU;
					createInfo.isMultiBuffered = true;
					const std::shared_ptr<Buffer> buffer = Buffer::Create(createInfo);
					m_BuffersByName[binding.buffer->name] = buffer;

					m_MaterialInfoIntermediate.materialBuffer[pass->GetId()] = buffer;
				}
			}
		}
	}

	for (const auto& [passName, uniformInfo] : createInfo.uniformInfos)
	{
		if (uniformInfo.texturesByName.empty() && uniformInfo.uniformBuffersByName.empty())
		{
			continue;
		}

		const std::shared_ptr<UniformWriter> uniformWriter = GetUniformWriter(passName);
		if (uniformWriter)
		{
			for (const auto& [name, filepath] : uniformInfo.texturesByName)
			{
				std::shared_ptr<Texture> texture = TextureManager::GetInstance().Load(filepath);
				uniformWriter->WriteTextureToAllFrames(name, texture);
			}
			uniformWriter->Flush();
		}

		for (const auto& [uniformBufferName, bufferInfo] : uniformInfo.uniformBuffersByName)
		{
			for (auto const& [loadedValueName, loadedValue] : bufferInfo.floatValuesByName)
			{
				WriteToBuffer(uniformBufferName, loadedValueName, loadedValue);
			}

			for (auto const& [loadedValueName, loadedValue] : bufferInfo.intValuesByName)
			{
				WriteToBuffer(uniformBufferName, loadedValueName, loadedValue);
			}

			for (auto const& [loadedValueName, loadedValue] : bufferInfo.vec4ValuesByName)
			{
				WriteToBuffer(uniformBufferName, loadedValueName, loadedValue);
			}

			for (auto const& [loadedValueName, loadedValue] : bufferInfo.vec3ValuesByName)
			{
				WriteToBuffer(uniformBufferName, loadedValueName, loadedValue);
			}

			for (auto const& [loadedValueName, loadedValue] : bufferInfo.vec2ValuesByName)
			{
				WriteToBuffer(uniformBufferName, loadedValueName, loadedValue);
			}

			for (const auto& [name, filepath] : bufferInfo.texturesByName)
			{
				std::shared_ptr<Texture> texture = TextureManager::GetInstance().Load(filepath);
				const int index = BindBindlessTexture(texture);
				WriteToBuffer(uniformBufferName, name, index);
			}

			device->ForEachFrame([this, uniformBufferName]()
			{
				GetBuffer(uniformBufferName)->Flush();
			});
		}
	}

	ReloadMaterialInfoBuffer();
}

void Material::ReloadMaterialInfoBuffer()
{
	if (!m_DirtyOption && m_MaterialInfoBuffer)
	{
		return;
	}

	m_DirtyOption = false;

	if (!m_MaterialInfoBuffer)
	{
		Buffer::CreateInfo createInfo{};
		createInfo.instanceSize = sizeof(MaterialInfo);
		createInfo.instanceCount = 1;
		createInfo.usages = { Buffer::Usage::STORAGE_BUFFER };
		createInfo.memoryType = MemoryType::GPU;
		createInfo.isMultiBuffered = true;
		m_MaterialInfoBuffer = Buffer::Create(createInfo);
	}

	device->ForEachFrame([this]()
	{
		MaterialInfo materialInfo{};
		materialInfo.pipelineFlags = 0;
		materialInfo.baseMaterialInfoBuffer = m_BaseMaterial->GetBaseMaterialInfoBuffer()->GetDeviceAddress().Get();

		for (const auto& [passName, pipeline] : m_BaseMaterial->GetPipelinesByPass())
		{
			std::shared_ptr<Pass> pass = RenderPassManager::GetInstance().GetPass(passName);
			if (IsPipelineEnabled(passName))
			{
				materialInfo.pipelineFlags |= 1 << pass->GetId();
			}
		}

		for (size_t i = 0; i < MAX_PIPELINE_COUNT_PER_MATERIAL; i++)
		{
			materialInfo.materialBuffer[i] = m_MaterialInfoIntermediate.materialBuffer[i] ?
				m_MaterialInfoIntermediate.materialBuffer[i]->GetDeviceAddress().Get() : 0;
		}

		m_MaterialInfoBuffer->WriteToBuffer((void*)&materialInfo, sizeof(MaterialInfo), 0);
		m_MaterialInfoBuffer->Flush();
	});

	m_MaterialInfoBuffer->ClearWrites();
}

void Material::FlushMaterialInfoBuffer()
{
	for (size_t i = 0; i < MAX_PIPELINE_COUNT_PER_MATERIAL; i++)
	{
		if (m_MaterialInfoIntermediate.materialBuffer[i])
		{
			m_MaterialInfoIntermediate.materialBuffer[i]->Flush();
		}
	}

	if (m_MaterialInfoBuffer)
	{
		m_MaterialInfoBuffer->Flush();
	}
}
