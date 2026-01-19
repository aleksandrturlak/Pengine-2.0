#include "BindlessUniformWriter.h"

#include "MaterialManager.h"

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

	{
		const auto& binding = uniformLayout->GetBindingByLocation(1);
		bindings.emplace_back(*binding);

		const size_t materialCount = binding->buffer->variables.begin()->count;
		const size_t materialSize = binding->buffer->variables.begin()->size / materialCount;

		m_BindlessMaterialBuffer = Buffer::Create(
			materialSize,
			materialCount,
			Buffer::Usage::STORAGE_BUFFER,
			MemoryType::CPU);

		m_BaseMaterialSize = materialSize;
		m_MaterialSlotManager.Initialize(materialCount);
	}
	
	const auto bindlessUniformLayout = UniformLayout::Create(bindings);
	m_BindlessUniformWriter = UniformWriter::Create(bindlessUniformLayout, false);
	m_BindlessUniformWriter->WriteBuffer("BindlessMaterials", m_BindlessMaterialBuffer);
	m_BindlessUniformWriter->Flush();
}

void BindlessUniformWriter::ShutDown()
{
	m_TexturesByIndex.clear();
	m_BindlessMaterialBuffer = nullptr;
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
		m_BindlessUniformWriter->WriteTexture(0, texture, index);
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

int BindlessUniformWriter::BindMaterial(const std::shared_ptr<Material>& material)
{
    if (material->GetBindlessIndex() != -1)
	{
		return material->GetBindlessIndex();
	}

	const int index = m_MaterialSlotManager.TakeSlot();
	material->SetBindlessIndex(index);

	m_MaterialsByIndex[index] = std::weak_ptr<Material>(material);

	return index;
}

void BindlessUniformWriter::UnBindMaterial(const std::shared_ptr<Material>& material)
{
	const int index = material->GetBindlessIndex();
	if (index == -1)
	{
		return;
	}

	m_MaterialSlotManager.FreeSlot(index);
	material->SetBindlessIndex(-1);

	m_MaterialsByIndex.erase(index);
}

std::shared_ptr<Material> BindlessUniformWriter::GetBindlessMaterial(const int index)
{
    auto materialsByIndex = m_MaterialsByIndex.find(index);
	if (materialsByIndex != m_MaterialsByIndex.end())
	{
		return materialsByIndex->second.lock();
	}

	return nullptr;
}

void BindlessUniformWriter::Flush()
{
	m_BindlessUniformWriter->Flush();
}
