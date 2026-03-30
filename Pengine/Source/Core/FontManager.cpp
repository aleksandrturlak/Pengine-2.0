#include "FontManager.h"

#include "Logger.h"
#include "TextureManager.h"

#include "../Utils/Utils.h"

#include <freetype/include/ft2build.h>
#include FT_FREETYPE_H

using namespace Pengine;

FontManager& FontManager::GetInstance()
{
	static FontManager fontManager;
	return fontManager;
}

void FontManager::Initialize()
{
	std::filesystem::path filepath;
	filepath = filepath / "Editor" / "Fonts" / "Calibri.ttf";
	LoadFont(filepath, 12);
	LoadFont(filepath, 14);
	LoadFont(filepath, 16);
	LoadFont(filepath, 18);
	LoadFont(filepath, 20);
	LoadFont(filepath, 24);
	LoadFont(filepath, 28);
	LoadFont(filepath, 36);
	LoadFont(filepath, 48);
	LoadFont(filepath, 60);
	LoadFont(filepath, 72);
}

std::shared_ptr<FontManager::Font> FontManager::LoadFont(const std::filesystem::path& filepath, const uint16_t fontSize)
{
	FT_Library library;

	auto error = FT_Init_FreeType(&library);
	if (error)
	{
		FATAL_ERROR("Failed to initialize font library!");
	}

	FT_Face face;
	error = FT_New_Face(library,
						filepath.string().c_str(),
						0,
						&face);
	if (error == FT_Err_Unknown_File_Format)
	{
		FATAL_ERROR("Failed to load font, the file format is not supported!");
	}
	else if (error)
	{
		FATAL_ERROR("Failed to load font!");
	}

	error = FT_Set_Pixel_Sizes(face, 0, fontSize);

	if (error == FT_Err_Unknown_File_Format)
	{
		FATAL_ERROR("Failed to set pixel sizes!");
	}

	// Full 256-entry table so unsigned char indexing in MeasureText is always in bounds
	const int glyphCount = 256;

	std::shared_ptr<Font> font = std::make_shared<Font>();
	font->glyphs.resize(glyphCount);

	// Round up so we never lose a pixel to truncation
	const int atlasSize = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(glyphCount) * fontSize * fontSize)));
	uint8_t* buffer = new uint8_t[atlasSize * atlasSize];

	std::memset(buffer, 0, atlasSize * atlasSize);

	const int padding = 2;
	int x = padding, y = padding;
	int rowMaxHeight = 0; // track per-row max, not global max

	for (int i = 0; i < glyphCount; ++i)
	{
		error = FT_Load_Char(face, i, FT_LOAD_RENDER);
		if (error)
		{
			FATAL_ERROR("Failed to load glyph!");
		}

		if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
		{
			error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
			if (error)
			{
				FATAL_ERROR("Failed to render glyph!");
			}
		}

		Font::Glyph glyph{};
		glyph.size    = { face->glyph->bitmap.width, face->glyph->bitmap.rows };
		glyph.bearing = { face->glyph->bitmap_left, face->glyph->bitmap_top };
		glyph.advance = face->glyph->advance.x >> 6;

		if (x + glyph.size.x + padding >= atlasSize)
		{
			x = padding;
			y += rowMaxHeight + padding;
			rowMaxHeight = 0; // reset for the new row
		}

		// Skip writing if the glyph would overflow the atlas vertically
		if (y + glyph.size.y < atlasSize)
		{
			glyph.uvMin = { (float)x / (float)atlasSize, (float)y / (float)atlasSize };
			glyph.uvMax = { (float)(x + glyph.size.x) / (float)atlasSize, (float)(y + glyph.size.y) / (float)atlasSize };

			for (int j = 0; j < glyph.size.y; ++j)
			{
				for (int k = 0; k < glyph.size.x; ++k)
				{
					const int srcIndex = j * glyph.size.x + k;
					const int dstIndex = (j + y) * atlasSize + (k + x);
					buffer[dstIndex] = face->glyph->bitmap.buffer[srcIndex];
				}
			}
		}

		rowMaxHeight = glm::max(rowMaxHeight, glyph.size.y);
		x += glyph.size.x + padding;

		font->glyphs[i] = glyph;
	}

	Texture::CreateInfo createInfo{};
	createInfo.aspectMask = Texture::AspectMask::COLOR;
	createInfo.instanceSize = sizeof(uint8_t);
	createInfo.filepath = filepath;
	createInfo.name = Utils::GetFilename(filepath);
	createInfo.format = Format::R8_UNORM;
	createInfo.size = { atlasSize, atlasSize };
	createInfo.usage = { Texture::Usage::SAMPLED, Texture::Usage::TRANSFER_DST };
	createInfo.data = buffer;

	font->atlas = Texture::Create(createInfo);

	delete[] buffer;

	font->name = createInfo.name;
	font->size = fontSize;
	font->id = m_FontIdCounter++;

	m_Fonts[createInfo.name][fontSize] = font;
	m_FontNamesById[font->id] = createInfo.name;

	FT_Done_Face(face);
	FT_Done_FreeType(library);

	return font;
}

glm::ivec2 FontManager::MeasureText(const std::string& fontName, const uint16_t fontSize, const std::string& text)
{
	if (!m_Fonts.contains(fontName))
	{
		Logger::Error("There is no any font with the name " + fontName + "!");
		return {};
	}

	if (!m_Fonts[fontName].contains(fontSize))
	{
		Logger::Error("The font with the name " + fontName + " doesn't have such font size loaded " + std::to_string(fontSize) + "!");
		return {};
	}

	std::shared_ptr<Font> font = m_Fonts[fontName][fontSize];

	glm::ivec2 size{};
	for (const auto& character : text)
	{
		const Font::Glyph& glyph = font->glyphs[static_cast<unsigned char>(character)];

		size.x += glyph.advance;
		const int descent = glm::max(0, glyph.size.y - glyph.bearing.y);
		size.y = glm::max<int>(size.y, glyph.bearing.y + descent);
	}

	return size;
}

Clay_Dimensions FontManager::ClayMeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void* userData)
{
	// Clay_StringSlice is not null-terminated — construct string with explicit length
	const std::string str(text.chars, text.length);
	const std::string fontName = FontManager::GetInstance().GetFontName(config->fontId);
	glm::ivec2 size = FontManager::GetInstance().MeasureText(fontName, config->fontSize, str);

	if (config->letterSpacing > 0 && text.length > 1)
		size.x += config->letterSpacing * (text.length - 1);

	// When lineHeight is 0, Clay uses the measured height directly as the line-box height.
	// Add default leading (~25% of font size) so wrapped lines aren't packed tight.
	const int lineHeight = config->lineHeight > 0
		? config->lineHeight
		: size.y + config->fontSize / 4;

	Clay_Dimensions dimensions{};
	dimensions.width  = static_cast<float>(size.x);
	dimensions.height = static_cast<float>(lineHeight);

	return dimensions;
}

std::shared_ptr<FontManager::Font> FontManager::GetFont(
	const std::string& fontName,
	const uint16_t fontSize) const
{
	if (!m_Fonts.contains(fontName))
	{
		Logger::Error("There is no any font with the name " + fontName + "!");
		return nullptr;
	}

	if (!m_Fonts.at(fontName).contains(fontSize))
	{
		Logger::Error("The font with the name " + fontName + " doesn't have such font size loaded " + std::to_string(fontSize) + "!");
		return nullptr;
	}

	return m_Fonts.at(fontName).at(fontSize);
}

const std::string FontManager::GetFontName(const uint16_t fontId) const
{
	if (!m_FontNamesById.contains(fontId))
	{
		Logger::Error("There is no any font with the id " + std::to_string(fontId) + "!");
		return {};
	}

	return m_FontNamesById.at(fontId);
}

void FontManager::ShutDown()
{
	m_Fonts.clear();
	m_FontNamesById.clear();
	m_FontIdCounter = 0;
}
