#include "BaseMaterial.h"

#include "../Core/Logger.h"
#include "../Core/Profiler.h"
#include "../Core/Serializer.h"
#include "../Core/TextureManager.h"
#include "../Core/BindlessUniformWriter.h"
#include "../EventSystem/EventSystem.h"
#include "../EventSystem/NextFrameEvent.h"

#include <algorithm>

using namespace Pengine;

std::shared_ptr<BaseMaterial> BaseMaterial::Create(
	const std::string& name,
	const std::filesystem::path& filepath,
	const CreateInfo& createInfo)
{
	PROFILER_SCOPE(__FUNCTION__);

	return std::make_shared<BaseMaterial>(name, filepath, createInfo);
}

std::shared_ptr<BaseMaterial> BaseMaterial::Load(const std::filesystem::path& filepath)
{
	PROFILER_SCOPE(__FUNCTION__);

	const CreateInfo createInfo = Serializer::LoadBaseMaterial(filepath);

	return Create(Utils::GetFilename(filepath), filepath, createInfo);
}

void BaseMaterial::Reload(const std::shared_ptr<BaseMaterial>& baseMaterial)
{
	PROFILER_SCOPE(__FUNCTION__);

	auto callback = [baseMaterial]()
	{
		baseMaterial->m_PipelinesByPass.clear();
		baseMaterial->m_UniformWriterByPass.clear();
		baseMaterial->m_BuffersByName.clear();
		baseMaterial->m_UniformsCache.clear();
		baseMaterial->CreateResources(Serializer::LoadBaseMaterial(baseMaterial->GetFilepath()));

	};

	std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, baseMaterial.get());
	EventSystem::GetInstance().SendEvent(event);
}

BaseMaterial::BaseMaterial(
	const std::string& name,
	const std::filesystem::path& filepath,
	const CreateInfo& createInfo)
	: Asset(name, filepath)
{
	CreateResources(createInfo);
}

BaseMaterial::~BaseMaterial()
{
}

std::shared_ptr<Pipeline> BaseMaterial::GetPipeline(const std::string& passName) const
{
	if (const auto pipelineByPass = m_PipelinesByPass.find(passName);
		pipelineByPass != m_PipelinesByPass.end())
	{
		return pipelineByPass->second;
	}
	
	return nullptr;
}

std::shared_ptr<UniformWriter> BaseMaterial::GetUniformWriter(const std::string& passName) const
{
	return Utils::Find(passName, m_UniformWriterByPass);
}

std::shared_ptr<Buffer> BaseMaterial::GetBuffer(const std::string& name) const
{
	return Utils::Find(name, m_BuffersByName);
}

bool BaseMaterial::GetUniformDetails(
	const std::string& uniformBufferName,
	const std::string& valueName,
	uint32_t& size,
	uint32_t& offset) const
{
	std::function<bool(
		const std::vector<ShaderReflection::ReflectVariable>&,
		std::string,
		const uint32_t,
		uint32_t&,
		uint32_t&)> findVariable = [&findVariable](
		const std::vector<ShaderReflection::ReflectVariable>& variables,
		std::string valueName,
		const uint32_t parentOffset,
		uint32_t& size,
		uint32_t& offset)
		{
			std::string searchingName;

			std::optional<int> arrayIndex;

			const size_t squareBracketOpenOffset = valueName.find_first_of('[');
			if (squareBracketOpenOffset != std::string::npos)
			{
				const size_t squareBracketCloseOffset = valueName.find_first_of(']');
				if (squareBracketCloseOffset == std::string::npos)
				{
					Logger::Warning("Failed to write to buffer: " + valueName + ", because missing closing ]");
					return false;
				}

				const std::string arrayIndexString = valueName.substr(squareBracketOpenOffset + 1, squareBracketCloseOffset - squareBracketOpenOffset - 1);
				arrayIndex = std::stoi(arrayIndexString);
			}

			const size_t dotOffset = valueName.find_first_of('.');
			if (dotOffset != std::string::npos)
			{
				if (squareBracketOpenOffset != std::string::npos)
				{
					searchingName = valueName.substr(0, squareBracketOpenOffset);

				}
				else
				{
					searchingName = valueName.substr(0, dotOffset);
				}

				valueName = valueName.substr(dotOffset + 1, valueName.size() - dotOffset - 1);
			}
			else
			{
				searchingName = valueName;
			}

			auto variable = std::find_if(
				variables.begin(),
				variables.end(),
				[searchingName](const ShaderReflection::ReflectVariable& variable)
				{ 
					return variable.name == searchingName;
				}
			);
			
			if (variable == variables.end())
			{
				return false;
			}

			uint32_t currentParentOffset = parentOffset;
			if (arrayIndex)
			{
				if (*arrayIndex > variable->count)
				{
					Logger::Warning("Failed to write to buffer: " + valueName + ", because the array index is greater than the count " + 
						std::to_string(variable->count) +  " of the array");
					return false;
				}

				const uint32_t instanceSize = variable->size / variable->count;
				currentParentOffset += instanceSize * *arrayIndex;
			}

			if (valueName == searchingName)
			{
				offset = variable->offset + parentOffset;
				size = variable->size;
				return true;
			}
			else if (!variable->variables.empty())
			{
				return findVariable(
					variable->variables,
					valueName,
					currentParentOffset + variable->offset,
					size,
					offset);
			}

			return false;
		};

	auto foundBufferCache = m_UniformsCache.find(uniformBufferName);
	if (foundBufferCache != m_UniformsCache.end())
	{
		auto foundValueCache = foundBufferCache->second.find(valueName);
		if (foundValueCache != foundBufferCache->second.end())
		{
			size = foundValueCache->second.first;
			offset = foundValueCache->second.second;
			return true;
		}
	}

	for (const auto& [passName, pipeline] : m_PipelinesByPass)
	{
		for (const auto& [set, layout] : pipeline->GetUniformLayouts())
		{
			for (const auto& binding : layout->GetBindings())
			{
				if (binding.buffer && binding.name == uniformBufferName)
				{
					const bool found = findVariable(binding.buffer->variables, valueName, 0, size, offset);

					if (found)
					{
						std::lock_guard<std::mutex> lock(m_UniformCacheMutex);
						m_UniformsCache[uniformBufferName][valueName] = std::make_pair(size, offset);
						return true;
					}
				}
			}
		}
	}

	size = 0;
	offset = 0;

	return false;
}

std::optional<ShaderReflection::ReflectVariable> BaseMaterial::GetUniformValue(
	const std::string& uniformBufferName,
	const std::string& valueName)
{
	std::function<std::optional<ShaderReflection::ReflectVariable>(
		const std::vector<ShaderReflection::ReflectVariable>&,
		std::string)> findVariable;

	findVariable = [&findVariable](
		const std::vector<ShaderReflection::ReflectVariable>& variables,
		std::string valueName) -> std::optional<ShaderReflection::ReflectVariable>
	{
		std::string searchingName;

		const size_t dotOffset = valueName.find_first_of('.');
		if (dotOffset != std::string::npos)
		{
			searchingName = valueName.substr(0, dotOffset);
			valueName = valueName.substr(dotOffset + 1, valueName.size() - dotOffset - 1);
		}
		else
		{
			searchingName = valueName;
		}

		auto variable = std::find_if(
			variables.begin(),
			variables.end(),
			[searchingName](const ShaderReflection::ReflectVariable& variable)
			{
				return variable.name == searchingName;
			}
		);

		if (variable == variables.end())
		{
			return std::nullopt;
		}

		if (!variable->variables.empty())
		{
			return findVariable(
				variable->variables,
				valueName);
		}

		return *variable;
	};

	for (const auto& [passName, pipeline] : m_PipelinesByPass)
	{
		for (const auto& [set, layout] : pipeline->GetUniformLayouts())
		{
			for (const auto& binding : layout->GetBindings())
			{
				if (binding.buffer && binding.name == uniformBufferName)
				{
					return findVariable(binding.buffer->variables, valueName);
				}
			}
		}
	}

	return std::nullopt;
}

std::shared_ptr<Texture> BaseMaterial::GetBindlessTexture(const int index) const
{
	auto bindlessTextureByIndex = m_BindlessTexturesByIndex.find(index);
	if (bindlessTextureByIndex != m_BindlessTexturesByIndex.end())
	{
		return bindlessTextureByIndex->second;
	}

	return nullptr;
}

int BaseMaterial::BindBindlessTexture(const std::shared_ptr<Texture>& texture)
{
    const int index = BindlessUniformWriter::GetInstance().BindTexture(texture);
	m_BindlessTexturesByIndex[index] = texture;
	return index;
}

void BaseMaterial::UnBindBindlessTexture(const std::shared_ptr<Texture>& texture)
{
	m_BindlessTexturesByIndex.erase(texture->GetBindlessIndex());
	BindlessUniformWriter::GetInstance().UnBindTexture(texture);
}

void BaseMaterial::CreateResources(const CreateInfo& createInfo)
{
	for (const GraphicsPipeline::CreateGraphicsInfo& pipelineCreateGraphicsInfo : createInfo.pipelineCreateGraphicsInfos)
	{
		const std::string passName = pipelineCreateGraphicsInfo.renderPass->GetName();

		try
		{
			const std::shared_ptr<Pipeline> pipeline = GraphicsPipeline::Create(pipelineCreateGraphicsInfo);
			CreatePipelineResources(passName, pipeline, pipelineCreateGraphicsInfo.uniformInfo);
			m_PipelinesByPass[passName] = pipeline;
		}
		catch (const std::exception&)
		{
			m_PipelinesByPass[passName] = nullptr;
		}
	}

	for (const ComputePipeline::CreateComputeInfo& pipelineCreateComputeInfo : createInfo.pipelineCreateComputeInfos)
	{
		const std::string passName = pipelineCreateComputeInfo.passName;

		try
		{
			const std::shared_ptr<Pipeline> pipeline = ComputePipeline::Create(pipelineCreateComputeInfo);
			CreatePipelineResources(passName, pipeline, pipelineCreateComputeInfo.uniformInfo);
			m_PipelinesByPass[passName] = pipeline;
		}
		catch (const std::exception&)
		{
			m_PipelinesByPass[passName] = nullptr;
		}
	}

	{
		Buffer::CreateInfo createInfo{};
		createInfo.instanceSize = sizeof(BaseMaterialInfoBuffer);
		createInfo.instanceCount = 1;
		createInfo.usages = { Buffer::Usage::STORAGE_BUFFER };
		createInfo.memoryType = MemoryType::CPU;
		createInfo.isMultiBuffered = true;
		m_BaseMaterialInfoBuffer = Buffer::Create(createInfo);
	}
}

void BaseMaterial::CreatePipelineResources(
	const std::string& passName,
	std::shared_ptr<Pipeline> pipeline,
	const Pipeline::UniformInfo& uniformInfo)
{
	auto baseMaterialIndex = pipeline->GetDescriptorSetIndexByType(Pipeline::DescriptorSetIndexType::BASE_MATERIAL, passName);
	if (baseMaterialIndex)
	{
		const std::shared_ptr<UniformLayout> uniformLayout = pipeline->GetUniformLayout(*baseMaterialIndex);
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
				uniformWriter->WriteBuffer(binding.buffer->name, buffer);
				uniformWriter->Flush();
			}
		}
	}

	if (uniformInfo.texturesByName.empty() && uniformInfo.uniformBuffersByName.empty())
	{
		return;
	}

	const std::shared_ptr<UniformWriter> uniformWriter = GetUniformWriter(passName);
	for (const auto& [name, filepath] : uniformInfo.texturesByName)
	{
		std::shared_ptr<Texture> texture = TextureManager::GetInstance().Load(filepath);
		uniformWriter->WriteTexture(name, texture);
	}
	uniformWriter->Flush();

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

		GetBuffer(uniformBufferName)->Flush();
	}
}
