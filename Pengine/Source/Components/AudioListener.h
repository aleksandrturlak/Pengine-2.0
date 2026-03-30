#pragma once

#include "../Core/Core.h"

namespace Pengine
{

	// Tag component — marks this entity as the audio listener.
	// Position and forward direction are read from the entity's Transform each frame.
	class PENGINE_API AudioListener
	{
		// Entt can't add zero size components.
		uint8_t padding;
	};

}
