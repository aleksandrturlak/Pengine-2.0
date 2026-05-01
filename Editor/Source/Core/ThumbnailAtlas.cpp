#include "ThumbnailAtlas.h"

#include "Core/Logger.h"
#include "Graphics/Texture.h"

#define DEFAULT_ATLAS_SIZE 2048

using namespace Pengine;

std::shared_ptr<class Texture> ThumbnailAtlas::Push(
	std::shared_ptr<class Texture> texture,
	TileInfo& tileInfo)
{
	const glm::ivec2 textureSize = texture->GetSize();
	
	if (textureSize.x != textureSize.y)
	{
		Logger::Error(
			Utils::Format("Editor: Can't push texture {} to thumbnail atlas, texture width and height have to be the same!", texture->GetFilepath().string()));
		return {};
	}

	tileInfo.atlasIndex = -1;
	for (size_t i = 0; i < m_Atlases.size(); i++)
	{
		if (m_Atlases[i].tileSize == textureSize.x && !m_Atlases[i].freeSlots.empty())
		{
			tileInfo.atlasIndex = i;
		}
	}

	Atlas* atlas = nullptr;

	if (tileInfo.atlasIndex >= 0)
	{
		atlas = &m_Atlases[tileInfo.atlasIndex];
	}
	else
	{
		tileInfo.atlasIndex = m_Atlases.size();

		atlas = &m_Atlases.emplace_back();
		atlas->tileSize = textureSize.x;
		atlas->count = DEFAULT_ATLAS_SIZE / atlas->tileSize;
		
		for (size_t i = 0; i < atlas->count * atlas->count; i++)
		{
			atlas->freeSlots.emplace(i);
		}

		Texture::CreateInfo createInfo{};
		createInfo.aspectMask = Texture::AspectMask::COLOR;
		createInfo.instanceSize = sizeof(uint8_t) * 4;
		createInfo.filepath = "ThumbnailAtlas";
		createInfo.name = "ThumbnailAtlas";
		createInfo.format = Format::R8G8B8A8_SRGB;
		createInfo.size = { DEFAULT_ATLAS_SIZE, DEFAULT_ATLAS_SIZE };
		createInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_DST };
		atlas->texture = Texture::Create(createInfo);
	}

	tileInfo.tileIndex = atlas->freeSlots.front();
	atlas->freeSlots.pop();

	const uint32_t tileIndexX = tileInfo.tileIndex % atlas->count;
	const uint32_t tileIndexY = tileInfo.tileIndex / atlas->count;

	Texture::Region region{};
	region.srcOffset = { 0, 0, 0 };
	region.dstOffset = { tileIndexX * atlas->tileSize, tileIndexY * atlas->tileSize, 0 };
	region.extent = { textureSize.x, textureSize.y, 1 };
	atlas->texture->Copy(texture, region);

	tileInfo.uvStart.x = (float)(tileIndexX * atlas->tileSize) / DEFAULT_ATLAS_SIZE;
	tileInfo.uvStart.y = (float)(tileIndexY * atlas->tileSize) / DEFAULT_ATLAS_SIZE;

	tileInfo.uvEnd.x = (float)(((tileIndexX + 1) * atlas->tileSize) - 1) / DEFAULT_ATLAS_SIZE;
	tileInfo.uvEnd.y = (float)(((tileIndexY + 1) * atlas->tileSize) - 1) / DEFAULT_ATLAS_SIZE;

	tileInfo.atlasId = atlas->texture->GetId();

	return atlas->texture;
}

std::shared_ptr<class Texture> ThumbnailAtlas::GetAtlas(int atlasIndex) const
{
	if (m_Atlases.empty() || atlasIndex < 0 || atlasIndex >= m_Atlases.size())
	{
		return nullptr;
	}

	return m_Atlases[atlasIndex].texture;
}

void ThumbnailAtlas::Free(int atlasIndex, int tileIndex)
{
	if (atlasIndex < 0 || tileIndex < 0)
	{
		return;
	}

	m_Atlases[atlasIndex].freeSlots.emplace(tileIndex);
}
