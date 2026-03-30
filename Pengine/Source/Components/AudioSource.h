#pragma once

#include "../Core/Core.h"

struct ma_sound;

namespace Pengine
{

	struct PENGINE_API AudioSource
	{
		// Path to the audio file (WAV, MP3, FLAC, OGG).
		std::string filePath;

		float volume = 1.0f;
		float pitch  = 1.0f;
		bool  loop   = false;

		bool playOnAwake = false;

		// If true the entity is automatically deleted once the sound finishes playing.
		bool oneShot = false;

		// false = 2-D (no distance attenuation), true = 3-D positional.
		bool spatialBlend = false;

		// 3-D attenuation distances (only used when spatialBlend == true).
		float minDistance = 1.0f;
		float maxDistance = 100.0f;

		ma_sound* m_Sound   = nullptr;
		bool      m_Started = false;
		bool      m_Dirty   = false;   // set true to ask AudioSystem to reload the sound
	};

}
