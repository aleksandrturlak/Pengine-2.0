// MA_IMPLEMENTATION must be defined in exactly one translation unit.
#define MA_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#include "AudioSystem.h"

#include "../Components/AudioListener.h"
#include "../Components/Transform.h"
#include "../Core/Scene.h"
#include "../Core/Logger.h"

namespace Pengine
{

	AudioSystem::AudioSystem()
	{
		m_Engine = new ma_engine();
		if (ma_engine_init(nullptr, m_Engine) != MA_SUCCESS)
		{
			delete m_Engine;
			m_Engine = nullptr;
			FATAL_ERROR("AudioSystem:Failed to initialize miniaudio engine!");
		}

		m_RemoveCallbacks["AudioSource"] = [this](std::shared_ptr<Entity> entity)
		{
			if (entity->HasComponent<AudioSource>())
			{
				UninitSource(entity->GetComponent<AudioSource>());
			}
		};
	}

	AudioSystem::~AudioSystem()
	{
		if (m_Engine)
		{
			ma_engine_uninit(m_Engine);
			delete m_Engine;
			m_Engine = nullptr;
		}
	}

	void AudioSystem::OnUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
	{
		if (!m_Engine)
			return;

		auto& registry = scene->GetRegistry();

		{
			auto listenerView = registry.view<AudioListener, Transform>();
			for (auto handle : listenerView)
			{
				const Transform& t = listenerView.get<Transform>(handle);

				const glm::vec3 pos = t.GetPosition();
				const glm::vec3 fwd = t.GetForward();
				const glm::vec3 up  = t.GetUp();

				ma_engine_listener_set_position(m_Engine, 0, pos.x, pos.y, pos.z);
				ma_engine_listener_set_direction(m_Engine, 0, fwd.x, fwd.y, fwd.z);
				ma_engine_listener_set_world_up(m_Engine, 0, up.x, up.y, up.z);

				break; // Only one listener is supported.
			}
		}

		{
			auto sourceView = registry.view<AudioSource>();
			for (auto handle : sourceView)
			{
				AudioSource& src = sourceView.get<AudioSource>(handle);

				if (src.filePath.empty())
					continue;

				if (src.m_Dirty && src.m_Sound)
				{
					UninitSource(src);
					src.m_Dirty = false;
				}

				if (!src.m_Sound)
				{
					const bool spatial = registry.all_of<Transform>(handle) && src.spatialBlend;
					InitSource(src, spatial);

					if (src.m_Sound && src.spatialBlend && registry.all_of<Transform>(handle))
					{
						const glm::vec3 pos = registry.get<Transform>(handle).GetPosition();
						ma_sound_set_position(src.m_Sound, pos.x, pos.y, pos.z);
					}
				}

				if (src.m_Sound && src.playOnAwake && !src.m_Started)
				{
					ma_sound_start(src.m_Sound);
					src.m_Started = true;
				}
			}
		}

		{
			auto spatialView = registry.view<AudioSource, Transform>();
			for (auto handle : spatialView)
			{
				AudioSource& src = spatialView.get<AudioSource>(handle);

				if (!src.m_Sound || !src.spatialBlend)
					continue;

				const glm::vec3 pos = spatialView.get<Transform>(handle).GetPosition();
				ma_sound_set_position(src.m_Sound, pos.x, pos.y, pos.z);
			}
		}

		{
			std::vector<std::shared_ptr<Entity>> toDelete;
			auto oneShotView = registry.view<AudioSource>();
			for (auto handle : oneShotView)
			{
				AudioSource& src = oneShotView.get<AudioSource>(handle);
				if (src.oneShot && src.m_Started && !IsPlaying(src) && registry.all_of<Transform>(handle))
					toDelete.push_back(registry.get<Transform>(handle).GetEntity());
			}
			for (auto& e : toDelete)
				scene->DeleteEntity(e);
		}
	}

	void AudioSystem::InitSource(AudioSource& source, bool spatial)
	{
		if (!m_Engine)
			return;

		source.m_Sound = new ma_sound();

		const uint32_t flags = spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;

		if (ma_sound_init_from_file(m_Engine, source.filePath.c_str(), flags, nullptr, nullptr, source.m_Sound) != MA_SUCCESS)
		{
			Logger::Error(std::format("AudioSystem:Failed to load {}", source.filePath));
			delete source.m_Sound;
			source.m_Sound = nullptr;
			return;
		}

		ma_sound_set_volume(source.m_Sound, source.volume);
		ma_sound_set_pitch(source.m_Sound, source.pitch);
		ma_sound_set_looping(source.m_Sound, source.loop ? MA_TRUE : MA_FALSE);

		if (spatial)
		{
			ma_sound_set_min_distance(source.m_Sound, source.minDistance);
			ma_sound_set_max_distance(source.m_Sound, source.maxDistance);
		}
	}

	void AudioSystem::UninitSource(AudioSource& source)
	{
		if (!source.m_Sound)
			return;

		ma_sound_uninit(source.m_Sound);
		delete source.m_Sound;
		source.m_Sound   = nullptr;
		source.m_Started = false;
	}

	void AudioSystem::Play(AudioSource& source)
	{
		if (!source.m_Sound)
			return;

		ma_sound_seek_to_pcm_frame(source.m_Sound, 0);
		ma_sound_start(source.m_Sound);
		source.m_Started = true;
	}

	void AudioSystem::Stop(AudioSource& source)
	{
		if (!source.m_Sound)
			return;

		ma_sound_stop(source.m_Sound);
		ma_sound_seek_to_pcm_frame(source.m_Sound, 0);
		source.m_Started = false;
	}

	void AudioSystem::Pause(AudioSource& source)
	{
		if (!source.m_Sound)
			return;

		ma_sound_stop(source.m_Sound);
	}

	void AudioSystem::SetVolume(AudioSource& source, float volume)
	{
		source.volume = volume;
		if (source.m_Sound)
			ma_sound_set_volume(source.m_Sound, volume);
	}

	void AudioSystem::SetPitch(AudioSource& source, float pitch)
	{
		source.pitch = pitch;
		if (source.m_Sound)
			ma_sound_set_pitch(source.m_Sound, pitch);
	}

	bool AudioSystem::IsPlaying(const AudioSource& source) const
	{
		if (!source.m_Sound)
			return false;

		return ma_sound_is_playing(source.m_Sound) == MA_TRUE;
	}

}
