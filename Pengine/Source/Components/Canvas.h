#pragma once

#include "../Core/Core.h"

#include <clay/clay.hpp>

namespace Pengine
{

	class PENGINE_API Canvas
	{
	public:

		struct Script
		{
			std::string name;

			clay::Context* context{};
		};

		std::vector<Script> scripts;

		std::vector<std::vector<clay::RenderCommand>> commands{};

		clay::MeasureTextFn measureText{};
		clay::QueryScrollFn queryScrollOffset{};

		bool drawInMainViewport = false;
		glm::ivec2 size{};

		std::shared_ptr<class FrameBuffer> frameBuffer;
	};

}
