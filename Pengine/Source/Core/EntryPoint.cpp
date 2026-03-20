#include "EntryPoint.h"

#include "AsyncAssetLoader.h"
#include "BindlessUniformWriter.h"
#include "FileFormatNames.h"
#include "FontManager.h"
#include "Input.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "RenderPassManager.h"
#include "SceneManager.h"
#include "Serializer.h"
#include "TextureManager.h"
#include "Time.h"
#include "ViewportManager.h"
#include "Viewport.h"
#include "WindowManager.h"
#include "ThreadPool.h"
#include "Profiler.h"
#include "Logger.h"

#include "../Components/Camera.h"
#include "../EventSystem/EventSystem.h"
#include "../Graphics/Device.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/RenderView.h"

using namespace Pengine;

EntryPoint::EntryPoint(Application* application)
	: m_Application(application)
{
	const auto [configGraphicsAPI] = Serializer::DeserializeEngineConfig(std::filesystem::path("Configs") / "Engine.yaml");
	graphicsAPI = configGraphicsAPI;
}

void LoadAllBaseMaterials(const std::filesystem::path& filepath)
{
	std::vector<std::filesystem::path> baseMaterialFilepaths;
	for (auto& directoryIter : std::filesystem::recursive_directory_iterator(filepath))
	{
		if (Utils::GetFileFormat(directoryIter.path()) == FileFormats::BaseMat())
		{
			baseMaterialFilepaths.emplace_back(directoryIter.path());
		}
	}

	std::atomic<int> baseMaterialsLoadedCount = 0;
	for (const auto& filepath : baseMaterialFilepaths)
	{
		AsyncAssetLoader::GetInstance().AsyncLoadBaseMaterial(filepath, [
			&baseMaterialsLoadedCount](std::weak_ptr<BaseMaterial> baseMaterial)
		{
			baseMaterialsLoadedCount.store(baseMaterialsLoadedCount.load() + 1);
		});
	}

	while (baseMaterialsLoadedCount.load() != baseMaterialFilepaths.size())
	{
		AsyncAssetLoader::GetInstance().Update();
	}
}

void CreateDefaultResources()
{
	{
		glm::vec2* vertices = new glm::vec2[4];
		vertices[0] = { -1.0f, -1.0f };
		vertices[1] = {  1.0f, -1.0f };
		vertices[2] = {  1.0f,  1.0f };
		vertices[3] = { -1.0f,  1.0f };

		std::vector<uint32_t> indices =
		{
			0, 1, 2, 2, 3, 0
		};

		Mesh::CreateInfo createInfo{};
		createInfo.filepath = "FullScreenQuad";
		createInfo.name = "FullScreenQuad";
		createInfo.indices = std::move(indices);
		createInfo.vertices = vertices;
		createInfo.vertexSize = sizeof(glm::vec2);
		createInfo.vertexCount = 4;
		createInfo.lods.emplace_back(Mesh::Lod{ .indexCount = createInfo.indices.size(), .indexOffset = 0, .distanceThreshold = 0.0f });
		createInfo.vertexLayouts =
		{
			VertexLayout(createInfo.vertexSize, "Position")
		};

		MeshManager::GetInstance().CreateMesh(createInfo);
	}

	{
		glm::vec3* vertices = new glm::vec3[8];
		vertices[0] = {	-1.0f, -1.0f, -1.0f, };  // 0: bottom-left-back
		vertices[1] = {	 1.0f, -1.0f, -1.0f, };  // 1: bottom-right-back
		vertices[2] = {	 1.0f,  1.0f, -1.0f, };  // 2: top-right-back
		vertices[3] = {	-1.0f,  1.0f, -1.0f, };  // 3: top-left-back
		vertices[4] = {	-1.0f, -1.0f,  1.0f, };  // 4: bottom-left-front
		vertices[5] = {	 1.0f, -1.0f,  1.0f, };  // 5: bottom-right-front
		vertices[6] = {	 1.0f,  1.0f,  1.0f, };  // 6: top-right-front
		vertices[7] = {	-1.0f,  1.0f,  1.0f  };  // 7: top-left-front

		std::vector<uint32_t> indices =
		{
			// Front face
			4, 5, 6, 6, 7, 4,
			// Back face
			1, 0, 3, 3, 2, 1,
			// Top face
			7, 6, 2, 2, 3, 7,
			// Bottom face
			0, 1, 5, 5, 4, 0,
			// Right face
			5, 1, 2, 2, 6, 5,
			// Left face
			0, 4, 7, 7, 3, 0
		};

		Mesh::CreateInfo createInfo{};
		createInfo.filepath = "UnitCube";
		createInfo.name = "UnitCube";
		createInfo.indices = std::move(indices);
		createInfo.vertices = vertices;
		createInfo.vertexSize = sizeof(glm::vec3);
		createInfo.vertexCount = 8;
		createInfo.lods.emplace_back(Mesh::Lod{ .indexCount = createInfo.indices.size(), .indexOffset = 0, .distanceThreshold = 0.0f });
		createInfo.vertexLayouts =
		{
			VertexLayout(createInfo.vertexSize, "Position")
		};

		MeshManager::GetInstance().CreateMesh(createInfo);
	}
}

void EntryPoint::Run() const
{
	Device::Create("Pengine");

	EventSystem& eventSystem = EventSystem::GetInstance();

	Serializer::GenerateFilesUUID(std::filesystem::current_path());

	RenderPassManager::GetInstance().Initialize();
	AsyncAssetLoader::GetInstance().Initialize();
	ThreadPool::GetInstance().Initialize(std::thread::hardware_concurrency() - 1);
	FontManager::GetInstance().Initialize();

	std::shared_ptr<Window> mainWindow = WindowManager::GetInstance().Create("Pengine", "Main",
		{ 1920, 1080 });

	mainWindow->GetViewportManager().Create("Main", { 800, 800 });

	WindowManager::GetInstance().SetCurrentWindow(mainWindow);

	BindlessUniformWriter::GetInstance().Initialize();
	TextureManager::GetInstance().CreateDefaultResources();
	TextureManager::GetInstance().LoadFromFolder(std::filesystem::path("Editor") / "Images");

	CreateDefaultResources();
	LoadAllBaseMaterials(std::filesystem::path("Materials"));

	m_Application->OnPreStart();

	std::shared_ptr<Renderer> renderer = Renderer::Create();
	
	m_Application->OnStart();

	try
	{
		while (mainWindow->IsRunning())
		{
			PROFILER_SCOPE(__FUNCTION__);

			Time::GetInstance().Update();
			AsyncAssetLoader::GetInstance().Update();

			{
				PROFILER_SCOPE("EventSystem::ProcessEvents");
				eventSystem.ProcessEvents();
			}

			{
				PROFILER_SCOPE("Application::OnUpdate");
				m_Application->OnUpdate();
			}

			if (const std::shared_ptr<Scene> scene = SceneManager::GetInstance().GetSceneByTag("Main"))
			{
				PROFILER_SCOPE("Scene::Update");
				scene->Update(Time::GetDeltaTime());
			}

			for (const auto& [windowName, window] : WindowManager::GetInstance().GetWindows())
			{
				if (!window->IsRunning())
				{
					WindowManager::GetInstance().Destroy(window);
					continue;
				}

				WindowManager::GetInstance().SetCurrentWindow(window);
				window->NewFrame();

				if (window->IsMinimized())
				{
					continue;
				}

				window->ImGuiBegin();
				for (const auto& [viewportName, viewport] : window->GetViewportManager().GetViewports())
				{
					if (const std::shared_ptr<Entity> camera = viewport->GetCamera().lock();
						camera && camera->HasComponent<Camera>())
					{
						Camera& cameraComponent = camera->GetComponent<Camera>();
						if (!cameraComponent.GetPassName().empty())
						{
							const std::shared_ptr<RenderView> renderView = cameraComponent.GetRendererTarget(viewportName);
							const std::shared_ptr<FrameBuffer> frameBuffer = renderView->GetFrameBuffer(cameraComponent.GetPassName());
							if (frameBuffer)
							{
								viewport->Update(frameBuffer->GetAttachment(cameraComponent.GetRenderTargetIndex()), window);
								continue;
							}

							const std::shared_ptr<Texture> texture = renderView->GetStorageImage(cameraComponent.GetPassName());
							if (texture)
							{
								viewport->Update(texture, window);
								continue;
							}
						}
					}

					viewport->Update(TextureManager::GetInstance().GetWhite(), window);
				}

				m_Application->OnImGuiUpdate();

				window->ImGuiEnd();

				drawCallCount = 0;
				triangleCount = 0;

				if (void* frame = window->BeginFrame())
				{
					std::map<std::shared_ptr<Scene>, std::vector<Renderer::RenderViewportInfo>> viewportsByScene;

					for (const auto& [viewportName, viewport] : window->GetViewportManager().GetViewports())
					{
						if (const std::shared_ptr<Entity> camera = viewport->GetCamera().lock())
						{
							if (camera->HasComponent<Camera>())
							{
								Renderer::RenderViewportInfo renderViewportInfo{};
								renderViewportInfo.camera = camera;
								renderViewportInfo.renderView = camera->GetComponent<Camera>().GetRendererTarget(viewportName);
								renderViewportInfo.projection = viewport->GetProjectionMat4();
								renderViewportInfo.size = viewport->GetSize();

								viewportsByScene[camera->GetScene()].emplace_back(renderViewportInfo);
							}
						}
					}

					Renderer::Update(
						frame,
						window,
						renderer,
						viewportsByScene);

					window->ImGuiRenderPass();
					window->EndFrame(frame);
				}
			}

			device->FlushDeletionQueue();

			++currentFrame;
		}
	}
	catch(const std::exception& e)
	{
		Logger::Log("Ending main loop due to an exception!");
	}

	m_Application->OnClose();

	Time::GetInstance().Update();
	AsyncAssetLoader::GetInstance().Update();

	eventSystem.ProcessEvents();

	AsyncAssetLoader::GetInstance().Shutdown();
	ThreadPool::GetInstance().Shutdown();
	SceneManager::GetInstance().ShutDown();
	MaterialManager::GetInstance().ShutDown();
	MeshManager::GetInstance().ShutDown();
	FontManager::GetInstance().ShutDown();
	TextureManager::GetInstance().ShutDown();
	RenderPassManager::GetInstance().ShutDown();
	BindlessUniformWriter::GetInstance().ShutDown();
	WindowManager::GetInstance().ShutDown();

	renderer = nullptr;
	mainWindow = nullptr;
	
	device->ShutDown();
}
