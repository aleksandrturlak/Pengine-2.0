#include "UISystem.h"

#include "../Core/Scene.h"
#include "../Core/Logger.h"
#include "../Core/FontManager.h"
#include "../Core/WindowManager.h"
#include "../Core/Viewport.h"
#include "../Core/ClayManager.h"

#include "../Components/Transform.h"

#include "../Graphics/FrameBuffer.h"

#define CLAY_IMPLEMENTATION
#include "../Components/Canvas.h"

using namespace Pengine;

namespace
{
	void HandleClayErrors(Clay_ErrorData errorData)
	{
		Logger::Error("Clay:" + std::string(errorData.errorText.chars));
	}
}

UISystem::UISystem()
{
	m_RemoveCallbacks[std::string(GetTypeName<Canvas>())] = [](std::shared_ptr<Entity> entity)
	{
		if (!entity || !entity->HasComponent<Canvas>())
		{
			return;
		}

		Canvas& canvas = entity->GetComponent<Canvas>();
		for (auto& script : canvas.scripts)
		{
			if (script.arenaMemory)
			{
				free(script.arenaMemory);
				script.arenaMemory = nullptr;
			}
			script.context = nullptr;
		}
	};
}

void UISystem::OnUpdate(const float deltaTime, std::shared_ptr<Scene> scene)
{
	const auto& view = scene->GetRegistry().view<Canvas>();
	for (const entt::entity entity : view)
	{
		Canvas& canvas = scene->GetRegistry().get<Canvas>(entity);
		Transform& transform = scene->GetRegistry().get<Transform>(entity);

		if (canvas.drawInMainViewport)
		{
			// TODO: Maybe make a variable with a viewport name.
			const std::shared_ptr<Viewport> viewport = WindowManager::GetInstance().GetWindowByName("Main")->GetViewportManager().GetViewport("Main");
			const glm::ivec2 viewportSize = viewport->GetSize();
			if (viewportSize.x != canvas.size.x || viewportSize.y != canvas.size.y)
			{
				canvas.size = viewportSize;
			}
		}

		canvas.commands.clear();


		for (auto& script : canvas.scripts)
		{
			auto foundCallback = ClayManager::GetInstance().scriptsByName.find(script.name);
			if (foundCallback == ClayManager::GetInstance().scriptsByName.end())
			{
				return;
			}

			if (!script.context)
			{
				Clay_SetCurrentContext(nullptr);
				Clay_SetMaxElementCount(1024);

				const size_t totalMemorySize = Clay_MinMemorySize();
				void* arenaMemory = malloc(totalMemorySize);
				Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, arenaMemory);

				script.context = Clay_Initialize(arena, Clay_Dimensions{ (float)canvas.size.x, (float)canvas.size.y }, Clay_ErrorHandler{ HandleClayErrors });
				script.arenaMemory = arenaMemory;
				canvas.measureText = FontManager::GetInstance().ClayMeasureText;
				canvas.queryScrollOffset = [](uint32_t, void*) -> Clay_Vector2 { return { 0.0f, 0.0f }; };
			}

			ClayManager::Init(&canvas, script.context);
			const Clay_RenderCommandArray commands = foundCallback->second(&canvas, transform.GetEntity());
			if (commands.length > 0)
			{
				canvas.commands.emplace_back(commands);
			}
		}
	}
}
