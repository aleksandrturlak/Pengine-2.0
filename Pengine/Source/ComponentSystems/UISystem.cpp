#define CLAY_IMPLEMENTATION
#include "UISystem.h"

#include "../Core/Scene.h"
#include "../Core/Logger.h"
#include "../Core/FontManager.h"
#include "../Core/WindowManager.h"
#include "../Core/Viewport.h"

#include "../Components/Transform.h"
#include "../Components/Canvas.h"

#include "../Graphics/FrameBuffer.h"

using namespace Pengine;

std::unordered_map<std::string, UISystem::ScriptFn>& UISystem::Scripts()
{
	static std::unordered_map<std::string, ScriptFn> scripts;
	return scripts;
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
			delete script.context;
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
			auto& scripts = Scripts();
			auto foundCallback = scripts.find(script.name);
			if (foundCallback == scripts.end())
			{
				return;
			}

			if (!script.context)
			{
				script.context = new clay::Context({ (float)canvas.size.x, (float)canvas.size.y }, 1024);
				canvas.measureText = FontManager::GetInstance().ClayMeasureText;
				canvas.queryScrollOffset = [](uint32_t, void*) -> clay::Vector2 { return { 0.0f, 0.0f }; };
				script.context->setMeasureTextFunction(canvas.measureText);
				script.context->setQueryScrollOffsetFunction(canvas.queryScrollOffset);
			}

			script.context->setLayoutDimensions({ (float)canvas.size.x, (float)canvas.size.y });
			std::vector<clay::RenderCommand> commands = foundCallback->second(&canvas, script.context, transform.GetEntity());
			if (!commands.empty())
			{
				canvas.commands.emplace_back(std::move(commands));
			}
		}
	}
}
