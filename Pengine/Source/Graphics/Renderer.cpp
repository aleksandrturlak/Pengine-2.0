#include "Renderer.h"

#include "../Core/Logger.h"
#include "../Core/RenderPassManager.h"
#include "../Core/RenderPassOrder.h"
#include "../Core/Scene.h"
#include "../Core/Serializer.h"
#include "../Core/Profiler.h"
#include "../Vulkan/VulkanRenderer.h"
#include "../Vulkan/VulkanWindow.h"

#include <filesystem>

using namespace Pengine;

std::shared_ptr<Renderer> Renderer::Create()
{
	if (graphicsAPI == GraphicsAPI::Vk)
	{
		return std::make_shared<Vk::VulkanRenderer>();
	}

	FATAL_ERROR("Failed to create the renderer, no graphics API implementation");
	return nullptr;
}

void Renderer::Update(
	void* frame,
	const std::shared_ptr<Window>& window,
	const std::shared_ptr<Renderer>& renderer,
	const std::map<std::shared_ptr<Scene>, std::vector<RenderViewportInfo>>& viewportsByScene)
{
	PROFILER_SCOPE(__FUNCTION__);

	for (const auto& [scene, viewports] : viewportsByScene)
	{
		if (!scene)
		{
			return;
		}

		if (!scene->GetRenderView())
		{
			scene->SetRenderView(RenderView::Create(passPerSceneOrder, { 0, 0 }));
		}

		Pass::RenderCallbackInfo renderInfo{};
		renderInfo.renderer = renderer;
		renderInfo.camera = nullptr;
		renderInfo.window = window;
		renderInfo.scene = scene;
		renderInfo.projection = glm::mat4(1.0f);
		renderInfo.frame = frame;
		renderInfo.viewportSize = { 0, 0 };

		renderer->BeginCommandLabel("Scene: " + scene->GetName(), glm::vec3(1.0f, 0.75f, 0.0f), frame);

		RenderPassManager::ProcessEntities(renderInfo);

		for (const auto& type : passPerSceneOrder)
		{
			const std::shared_ptr<Pass> pass = RenderPassManager::GetInstance().GetPass(type);
			if (!pass)
			{
				continue;
			}

			if (!pass->m_IsInitialized)
			{
				if (pass->m_CreateCallback)
				{
					pass->m_CreateCallback(pass.get());
					pass->m_IsInitialized = true;
				}
			}

			if (pass->GetType() == Pass::Type::GRAPHICS)
			{
				renderInfo.renderPass = std::dynamic_pointer_cast<RenderPass>(pass);
			}

			pass->Execute(renderInfo);
		}

		for (const auto& viewport : viewports)
		{
			renderInfo.camera = viewport.camera;
			renderInfo.projection = viewport.projection;
			renderInfo.viewportSize = viewport.size;
			renderInfo.renderView = viewport.renderView;

			RenderPassManager::PrepareUniformsPerViewportBeforeDraw(renderInfo);

			renderer->BeginCommandLabel("Camera: " + viewport.camera->GetName(), glm::vec3(1.0f, 0.75f, 0.0f), frame);

			for (const auto& type : passPerViewportOrder)
			{
				const std::shared_ptr<Pass> pass = RenderPassManager::GetInstance().GetPass(type);
				if (!pass)
				{
					continue;
				}

				if (!pass->m_IsInitialized)
				{
					if (pass->m_CreateCallback)
					{
						pass->m_CreateCallback(pass.get());
						pass->m_IsInitialized = true;
					}
				}

				if (pass->GetType() == Pass::Type::GRAPHICS)
				{
					renderInfo.renderPass = std::dynamic_pointer_cast<RenderPass>(pass);
				}

				pass->Execute(renderInfo);
			}

			renderer->EndCommandLabel(frame);
		}

		renderer->EndCommandLabel(frame);
	}
}
