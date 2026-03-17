#include <gtest/gtest.h>

#include "EventSystem/EventSystem.h"
#include "Core/Logger.h"

using namespace Pengine;

TEST(Event, Construct)
{
	int sender = 42;
	auto event = std::make_shared<Event>(Event::Type::OnUpdate, &sender);

	EXPECT_EQ(event->GetType(), Event::Type::OnUpdate);
	EXPECT_EQ(event->GetSender(), &sender);
	EXPECT_FALSE(event->GetSendedOnce());
}

TEST(Event, SendedOnce)
{
	auto event = std::make_shared<Event>(Event::Type::OnClose, nullptr, true);
	EXPECT_TRUE(event->GetSendedOnce());
}

TEST(EventSystem, RegisterAndProcess)
{
	try
	{
		EventSystem& es = EventSystem::GetInstance();
		es.ClearEvents();

		int callCount = 0;
		int client = 0;

		es.RegisterClient(Event::Type::OnUpdate, { &client, [&](std::shared_ptr<Event>) { callCount++; } });

		es.SendEvent(std::make_shared<Event>(Event::Type::OnUpdate, nullptr));
		es.ProcessEvents();

		EXPECT_EQ(callCount, 1);

		es.UnregisterAll(&client);
		es.ClearEvents();
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EventSystem, CallbackReceivesCorrectEvent)
{
	try
	{
		EventSystem& es = EventSystem::GetInstance();
		es.ClearEvents();

		int client = 0;
		Event::Type receivedType = Event::Type::OnStart;

		es.RegisterClient(Event::Type::OnResize, { &client, [&](std::shared_ptr<Event> e) { receivedType = e->GetType(); } });

		es.SendEvent(std::make_shared<Event>(Event::Type::OnResize, nullptr));
		es.ProcessEvents();

		EXPECT_EQ(receivedType, Event::Type::OnResize);

		es.UnregisterAll(&client);
		es.ClearEvents();
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EventSystem, UnregisterClient)
{
	try
	{
		EventSystem& es = EventSystem::GetInstance();
		es.ClearEvents();

		int callCount = 0;
		int client = 0;

		es.RegisterClient(Event::Type::OnUpdate, { &client, [&](std::shared_ptr<Event>) { callCount++; } });
		es.UnregisterClient(Event::Type::OnUpdate, &client);

		es.SendEvent(std::make_shared<Event>(Event::Type::OnUpdate, nullptr));
		es.ProcessEvents();

		EXPECT_EQ(callCount, 0);

		es.ClearEvents();
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EventSystem, UnregisterAll)
{
	try
	{
		EventSystem& es = EventSystem::GetInstance();
		es.ClearEvents();

		int callCount = 0;
		int client = 0;

		es.RegisterClient(Event::Type::OnUpdate, { &client, [&](std::shared_ptr<Event>) { callCount++; } });
		es.RegisterClient(Event::Type::OnClose,  { &client, [&](std::shared_ptr<Event>) { callCount++; } });
		es.RegisterClient(Event::Type::OnResize, { &client, [&](std::shared_ptr<Event>) { callCount++; } });

		es.UnregisterAll(&client);

		es.SendEvent(std::make_shared<Event>(Event::Type::OnUpdate, nullptr));
		es.SendEvent(std::make_shared<Event>(Event::Type::OnClose,  nullptr));
		es.SendEvent(std::make_shared<Event>(Event::Type::OnResize, nullptr));
		es.ProcessEvents();

		EXPECT_EQ(callCount, 0);

		es.ClearEvents();
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EventSystem, AlreadyRegistered)
{
	try
	{
		EventSystem& es = EventSystem::GetInstance();
		int client = 0;

		// Clean state
		es.UnregisterAll(&client);

		EXPECT_FALSE(es.AlreadyRegistered(Event::Type::OnUpdate, &client));

		es.RegisterClient(Event::Type::OnUpdate, { &client, [](std::shared_ptr<Event>) {} });
		EXPECT_TRUE(es.AlreadyRegistered(Event::Type::OnUpdate, &client));

		es.UnregisterAll(&client);
		es.ClearEvents();
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EventSystem, ClearEventsBeforeProcess)
{
	try
	{
		EventSystem& es = EventSystem::GetInstance();
		es.ClearEvents();

		int callCount = 0;
		int client = 0;

		es.RegisterClient(Event::Type::OnUpdate, { &client, [&](std::shared_ptr<Event>) { callCount++; } });

		es.SendEvent(std::make_shared<Event>(Event::Type::OnUpdate, nullptr));
		es.ClearEvents();
		es.ProcessEvents();

		EXPECT_EQ(callCount, 0);

		es.UnregisterAll(&client);
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EventSystem, MultipleClients)
{
	try
	{
		EventSystem& es = EventSystem::GetInstance();
		es.ClearEvents();

		int countA = 0, countB = 0;
		int clientA = 0, clientB = 1;

		es.RegisterClient(Event::Type::OnUpdate, { &clientA, [&](std::shared_ptr<Event>) { countA++; } });
		es.RegisterClient(Event::Type::OnUpdate, { &clientB, [&](std::shared_ptr<Event>) { countB++; } });

		es.SendEvent(std::make_shared<Event>(Event::Type::OnUpdate, nullptr));
		es.ProcessEvents();

		EXPECT_EQ(countA, 1);
		EXPECT_EQ(countB, 1);

		es.UnregisterAll(&clientA);
		es.UnregisterAll(&clientB);
		es.ClearEvents();
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}

TEST(EventSystem, ProcessingDisabled)
{
	try
	{
		EventSystem& es = EventSystem::GetInstance();
		es.ClearEvents();

		int callCount = 0;
		int client = 0;

		es.RegisterClient(Event::Type::OnUpdate, { &client, [&](std::shared_ptr<Event>) { callCount++; } });

		es.SetProcessingEventsEnabled(false);
		es.SendEvent(std::make_shared<Event>(Event::Type::OnUpdate, nullptr));
		es.ProcessEvents();

		EXPECT_EQ(callCount, 0);

		es.SetProcessingEventsEnabled(true);
		es.UnregisterAll(&client);
		es.ClearEvents();
	}
	catch (const std::exception& e)
	{
		Logger::Error(e.what());
		FAIL();
	}
}
