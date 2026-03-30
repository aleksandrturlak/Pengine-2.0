#pragma once

#include "../Core/Core.h"
#include "ComponentSystem.h"
#include "../Components/AudioSource.h"

struct ma_engine;

namespace Pengine
{

	class PENGINE_API AudioSystem : public ComponentSystem
	{
	public:
		AudioSystem();
		virtual ~AudioSystem() override;

		virtual void OnUpdate(const float deltaTime, std::shared_ptr<class Scene> scene) override;

		virtual std::map<std::string, std::function<void(std::shared_ptr<class Entity>)>> GetRemoveCallbacks() override
		{
			return m_RemoveCallbacks;
		}

		void Play(AudioSource& source);
		void Stop(AudioSource& source);
		void Pause(AudioSource& source);
		void SetVolume(AudioSource& source, float volume);
		void SetPitch(AudioSource& source, float pitch);
		[[nodiscard]] bool IsPlaying(const AudioSource& source) const;

	private:
		void InitSource(AudioSource& source, bool spatial);
		void UninitSource(AudioSource& source);

		ma_engine* m_Engine = nullptr;

		std::map<std::string, std::function<void(std::shared_ptr<class Entity>)>> m_RemoveCallbacks;
	};

}
