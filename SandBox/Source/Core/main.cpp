#include "Core/EntryPoint.h"
#include "Core/Logger.h"
#include "Game/GameApplication.h"

int main()
{
	try
	{
		GameApplication application{};
		Pengine::EntryPoint entryPoint(&application);
		entryPoint.Run();
	}
	catch (const std::runtime_error& runtimeError)
	{
		Pengine::Logger::Error(runtimeError.what());
	}

	return 0;
}
