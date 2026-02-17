#include "BindlessUniformWriter.h"

#include "MaterialManager.h"
#include "MeshManager.h"

#include "../Graphics/Device.h"
#include "../Graphics/Texture.h"
#include "../Graphics/UniformLayout.h"
#include "../Graphics/UniformWriter.h"

using namespace Pengine;

BindlessUniformWriter& BindlessUniformWriter::GetInstance()
{
	static BindlessUniformWriter bindlessUniformWriter;
	return bindlessUniformWriter;
}

void BindlessUniformWriter::Initialize()
{
	std::vector<ShaderReflection::ReflectDescriptorSetBinding> bindings;
	
	m_BaseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(std::filesystem::path("Materials") / "DefaultReflection.basemat");
	const auto& pipeline = m_BaseMaterial->GetPipeline(DefaultReflection);
	const auto& uniformLayout = pipeline->GetUniformLayout(1);
	
	{
		const auto& binding = uniformLayout->GetBindingByLocation(0);
		bindings.emplace_back(*binding);

		m_TextureSlotManager.Initialize(binding->count);
	}
	
	const auto bindlessUniformLayout = UniformLayout::Create(bindings);
	m_BindlessUniformWriter = UniformWriter::Create(bindlessUniformLayout, true);
}

void BindlessUniformWriter::ShutDown()
{
	m_TexturesByIndex.clear();
	m_BindlessUniformWriter = nullptr;
	m_BaseMaterial = nullptr;
}

int BindlessUniformWriter::BindTexture(const std::shared_ptr<Texture>& texture)
{
	if (texture->GetBindlessIndex() > 0)
	{
		return texture->GetBindlessIndex();
	}

	const int index = m_TextureSlotManager.TakeSlot();
	texture->SetBindlessIndex(index);

	m_TexturesByIndex[index] = std::weak_ptr<Texture>(texture);

	// Note: Slot 0 is supposed to be pink texture and always taken!
	if (index > 0)
	{
		m_BindlessUniformWriter->WriteTextureToAllFrames(0, { texture, 0 }, index);
	}

	return index;
}

void BindlessUniformWriter::UnBindTexture(const std::shared_ptr<Texture>& texture)
{
	const int index = texture->GetBindlessIndex();
	if (index == 0)
	{
		return;
	}

	m_TextureSlotManager.FreeSlot(index);
	texture->SetBindlessIndex(0);

	m_TexturesByIndex.erase(index);
}

std::shared_ptr<Texture> BindlessUniformWriter::GetBindlessTexture(const int index)
{
	auto textureByIndex = m_TexturesByIndex.find(index);
	if (textureByIndex != m_TexturesByIndex.end())
	{
		return textureByIndex->second.lock();
	}

	return nullptr;
}

void BindlessUniformWriter::CreateBindlessEntitiesResources(
	std::shared_ptr<UniformWriter>& uniformWriter,
	std::shared_ptr<Buffer>& buffer)
{
	const auto& pipeline = m_BaseMaterial->GetPipeline(DefaultReflection);
	const auto& uniformLayout = pipeline->GetUniformLayout(3);
	const auto& binding = uniformLayout->GetBindingByLocation(0);

	{
		const size_t entityCount = binding->buffer->variables.begin()->count;
		const size_t entitySize = binding->buffer->variables.begin()->size / entityCount;

		assert(entitySize == sizeof(EntityInfo));

		Buffer::CreateInfo createInfo{};
		createInfo.instanceSize = entitySize;
		createInfo.instanceCount = entityCount;
		createInfo.usages = { Buffer::Usage::STORAGE_BUFFER };
		createInfo.memoryType = MemoryType::CPU;
		createInfo.isMultiBuffered = true;
		buffer = Buffer::Create(createInfo);

		Logger::Log(std::format("Bindless Entity Buffer Size: {} MB", (entitySize * entityCount * Vk::frameInFlightCount) / 1024.0f / 1024.0f));
	}

	const std::vector<ShaderReflection::ReflectDescriptorSetBinding> bindings = { *binding };
	const auto entityUniformLayout = UniformLayout::Create(bindings);
	uniformWriter = UniformWriter::Create(entityUniformLayout, true);
	uniformWriter->WriteBufferToAllFrames("BindlessEntities", buffer);
	uniformWriter->Flush();
}
