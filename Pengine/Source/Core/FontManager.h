#pragma once

#include "Core.h"

#include <clay/clay.hpp>

namespace Pengine
{

	class PENGINE_API FontManager
	{
	public:
		static FontManager& GetInstance();

		FontManager(const FontManager&) = delete;
		FontManager& operator=(const FontManager&) = delete;

		void Initialize();

		struct Font
		{
			struct Glyph
			{
				glm::ivec2 size;
				glm::ivec2 bearing;
				glm::vec2 uvMin;
				glm::vec2 uvMax;
				uint32_t advance;
			};

			std::vector<Glyph> glyphs;

			std::shared_ptr<class Texture> atlas;

			std::string name;
			uint16_t id = -1;
			uint16_t size = 0;
		};

		std::shared_ptr<Font> LoadFont(const std::filesystem::path& filepath, const uint16_t fontSize);

		glm::ivec2 MeasureText(const std::string& fontName, const uint16_t fontSize, const std::string& text);

		static clay::Dimensions ClayMeasureText(clay::StringSlice text, const clay::TextElementConfig* config, void* userData);

		std::shared_ptr<Font> GetFont(const std::string& fontName, const uint16_t fontSize) const;

		const std::string GetFontName(const uint16_t fontId) const;

		std::shared_ptr<Font> GetFont(const uint16_t fontId, const uint16_t fontSize) const
		{
			return GetFont(GetFontName(fontId), fontSize);
		}

		void ShutDown();

	private:
		FontManager() = default;
		~FontManager() = default;

		std::unordered_map<std::string, std::unordered_map<uint16_t, std::shared_ptr<Font>>> m_Fonts;
		std::unordered_map<uint16_t, std::string> m_FontNamesById;

		uint16_t m_FontIdCounter = 0;
	};

}
