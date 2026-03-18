#pragma once

#include "../Core/Core.h"

namespace Pengine
{

	class PENGINE_API Event
	{
	public:
		enum class Type
		{
			OnStart,
			OnUpdate,
			OnClose,
			OnResize,
			OnMainThreadProcess,
			OnNextFrame,
			OnSetScroll,
			OnCollisionEnter,
			OnCollisionStay,
			OnCollisionExit
		};

		[[nodiscard]] Type GetType() const { return m_Type; }

		[[nodiscard]] void* GetSender() const { return m_Sender; }

		[[nodiscard]] bool GetSendedOnce() const { return m_SendedOnce; }

		Event(const Type type, void* sender, const bool sendedOnce = false)
			: m_Sender(sender)
			, m_Type(type)
			, m_SendedOnce(sendedOnce)
		{
		}

		virtual ~Event() = default;

	private:
		void* m_Sender = nullptr;
		Type m_Type = Type::OnStart;
		bool m_SendedOnce = false;
	};

}