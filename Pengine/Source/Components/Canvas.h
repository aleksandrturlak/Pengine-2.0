#pragma once

#include "../Core/Core.h"

#include <clay/clay.h>

namespace Pengine
{

	class PENGINE_API Canvas
	{
	public:

		struct Script
		{
			std::string name;

			Clay_Context* context{};
		void* arenaMemory{};
		};

		std::vector<Script> scripts;

		std::vector<Clay_RenderCommandArray> commands{};

		Clay_Dimensions (*measureText)(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);
		Clay_Vector2 (*queryScrollOffset)(uint32_t elementId, void *userData);

		bool drawInMainViewport = false;
		glm::ivec2 size{};

		std::shared_ptr<class FrameBuffer> frameBuffer;
	};

}
