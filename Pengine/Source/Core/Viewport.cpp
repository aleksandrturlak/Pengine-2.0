#include "Viewport.h"

#include "FileFormatNames.h"
#include "Logger.h"
#include "SceneManager.h"
#include "Serializer.h"
#include "ViewportManager.h"
#include "Input.h"
#include "KeyCode.h"
#include "Raycast.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "Window.h"

#include "../Components/Camera.h"
#include "../Components/Transform.h"
#include "../Components/Renderer3D.h"
#include "../Components/SkeletalAnimator.h"
#include "../EventSystem/EventSystem.h"
#include "../EventSystem/NextFrameEvent.h"
#include "../EventSystem/ResizeEvent.h"
#include "../Utils/Utils.h"

using namespace Pengine;

Viewport::Viewport(std::string name, const glm::ivec2& size)
	: m_Name(std::move(name))
	, m_Size(size)
{
	Resize(size);
}

void Viewport::Update(const std::shared_ptr<Texture>& viewportTexture, std::shared_ptr<Window> window)
{
	UpdateProjectionMat4();

	if (IsHeadLess())
	{
		return;
	}

	if (!viewportTexture)
	{
		FATAL_ERROR("Viewport texture is nullptr!");
	}

	if (!m_IsOpened)
	{
		window->GetViewportManager().Destroy(shared_from_this());
		return;
	}

	ImGui::Begin(m_Name.c_str(), &m_IsOpened);

	ImGui::Image(viewportTexture->GetId((Vk::frameInFlightIndex + 1) % Vk::frameInFlightCount), ImVec2(m_Size.x, m_Size.y));

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
		{
			std::wstring dragDropSource(static_cast<const wchar_t*>(payload->Data));
			dragDropSource.resize(payload->DataSize / sizeof(wchar_t));
			std::filesystem::path path = dragDropSource;

			std::shared_ptr<Scene> scene;
			if (const std::shared_ptr<Entity> camera = m_Camera.lock())
			{
				scene = camera->GetScene();
			}

			if (scene && FileFormats::Prefab() == Utils::GetFileFormat(path))
			{
				auto callback = [this, weakScene = std::weak_ptr<Scene>(scene), path]()
				{
					if (const std::shared_ptr<Scene> currentScene = weakScene.lock())
					{
						const std::shared_ptr<Entity> entity = Serializer::DeserializePrefab(path, currentScene);

						if (std::shared_ptr<Entity> camera = m_Camera.lock())
						{
							if (camera)
							{
								const glm::vec3 ray = GetMouseRay(m_MousePosition);

								const glm::vec3 start = camera->GetComponent<Transform>().GetPosition();
								const auto hits = Raycast::RaycastScene(camera->GetScene(), start, ray, camera->GetComponent<Camera>().GetZFar());
								if (!hits.empty())
								{
									entity->GetComponent<Transform>().Translate(hits.begin()->first.point);
								}
							}
						}
					}
				};

				std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
				EventSystem::GetInstance().SendEvent(event);
			}
			else if (FileFormats::Scene() == Utils::GetFileFormat(path))
			{
				auto callback = [weakScene = std::weak_ptr<Scene>(scene), path]()
				{
					if (std::shared_ptr<Scene> currentScene = weakScene.lock())
					{
						SceneManager::GetInstance().Delete(currentScene->GetName());
					}

					Serializer::DeserializeScene(path);
				};

				std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
				EventSystem::GetInstance().SendEvent(event);
			}
			else if (FileFormats::Mat() == Utils::GetFileFormat(path))
			{
				auto callback = [this, path]()
				{
					if (std::shared_ptr<Entity> camera = m_Camera.lock())
					{
						if (camera)
						{
							const glm::vec3 ray = GetMouseRay(m_MousePosition);

							const auto hits = Raycast::RaycastScene(camera->GetScene(), camera->GetComponent<Transform>().GetPosition(), ray, camera->GetComponent<Camera>().GetZFar());
							if (!hits.empty())
							{
								std::shared_ptr<Entity> entity = hits.begin()->second;
								if (entity->HasComponent<Renderer3D>())
								{
									std::shared_ptr<Material> material = MaterialManager::GetInstance().LoadMaterial(path);
									if (material)
									{
										entity->GetComponent<Renderer3D>().material = material;
									}
								}
							}
						}
					}
				};

				std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
				EventSystem::GetInstance().SendEvent(event);
			}
			else if (FileFormats::Mesh() == Utils::GetFileFormat(path))
			{
				auto callback = [this, path]()
				{
					if (std::shared_ptr<Entity> camera = m_Camera.lock())
					{
						if (camera)
						{
							const glm::vec3 ray = GetMouseRay(m_MousePosition);

							const glm::vec3 start = camera->GetComponent<Transform>().GetPosition();
							const auto hits = Raycast::RaycastScene(camera->GetScene(), start, ray, camera->GetComponent<Camera>().GetZFar());
							glm::vec3 position{};
							if (!hits.empty())
							{
								position = hits.begin()->first.point;
							}

							std::shared_ptr<Mesh> mesh = MeshManager::GetInstance().LoadMesh(path);
							if (mesh)
							{
								const std::shared_ptr<Entity> entity = camera->GetScene()->CreateEntity(mesh->GetName());
								entity->AddComponent<Transform>(entity).Translate(position);
								Renderer3D& r3d = entity->AddComponent<Renderer3D>();
								r3d.mesh = mesh;

								if (mesh->GetType() == Mesh::Type::SKINNED)
								{
									entity->AddComponent<SkeletalAnimator>();
								}
								
								r3d.material = MaterialManager::GetInstance().LoadMaterial(std::filesystem::path("Materials") / "MeshBaseDoubleSided.mat");
							}
						}
					}
				};

				std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
				EventSystem::GetInstance().SendEvent(event);
			}
		}

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT"))
		{
			UUID* uuidPtr = (UUID*)payload->Data;
			auto callback = [this, uuid = *uuidPtr]()
			{
				if (std::shared_ptr<Entity> entity = SceneManager::GetInstance().GetSceneByTag("Main")->FindEntityByUUID(uuid))
				{
					if (entity->HasComponent<Camera>())
					{
						SetCamera(entity);
					}
				}
			};

			std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
			EventSystem::GetInstance().SendEvent(event);
		}

		ImGui::EndDragDropTarget();
	}

	m_IsHovered = ImGui::IsWindowHovered();
	m_IsFocused = ImGui::IsWindowFocused();

	if (m_Size.x != ImGui::GetWindowSize().x || m_Size.y != ImGui::GetWindowSize().y)
	{
		Resize(glm::ivec2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y));
	}

	m_Position = glm::vec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);
	glm::vec2 mousePosition = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
	m_MousePosition = mousePosition - m_Position;

	const std::shared_ptr<Entity> camera = m_Camera.lock();
	const std::shared_ptr<Scene> scene = camera ? camera->GetScene() : nullptr;
	if (m_DrawGizmosCallback)
	{
		if (camera)
		{
			m_DrawGizmosCallback(m_Position, m_Size, camera, m_ActiveGuizmo);
		}
		m_DrawGizmosCallback = {};
	}

	Input& input = Input::GetInstance(window.get());
	if (scene && IsFocused() && input.IsKeyDown(KeyCode::KEY_LEFT_CONTROL) && input.IsKeyPressed(KeyCode::KEY_A))
	{
		scene->GetSelectedEntities().clear();
		for (const std::shared_ptr<Entity> entity : scene->GetEntities())
		{
			scene->GetSelectedEntities().emplace(entity);
		}
	}

	if (!m_ActiveGuizmo && m_IsHovered && input.IsMousePressed(KeyCode::MOUSE_BUTTON_1))
	{
		if (camera)
		{
			const glm::vec3 ray = GetMouseRay(m_MousePosition);

			const auto hits = Raycast::RaycastScene(scene, camera->GetComponent<Transform>().GetPosition(), ray, camera->GetComponent<Camera>().GetZFar());
			if (!hits.empty())
			{
				std::shared_ptr<Entity> entity = hits.begin()->second;
				std::shared_ptr<Entity> parent = entity->GetTopEntity();
				if (entity != parent && !scene->GetSelectedEntities().count(parent))
				{
					entity = parent;
				}

				scene->GetSelectedEntities().clear();
				scene->GetSelectedEntities().emplace(entity);
			}
		}
	}

	ImGui::End();
}

void Viewport::SetCamera(const std::shared_ptr<Entity>& camera)
{
	std::shared_ptr<Entity> previousCamera = m_Camera.lock();
	if (previousCamera && previousCamera->HasComponent<Camera>())
	{
		Camera& cameraComponent = m_Camera.lock()->GetComponent<Camera>();
		cameraComponent.DeleteRenderView(m_Name);
	}

	m_Camera = camera;

	if (!camera)
	{
		return;
	}

	Camera& cameraComponent = camera->GetComponent<Camera>();
	cameraComponent.CreateRenderView(m_Name, m_Size);

	UpdateProjectionMat4();
}

glm::vec3 Viewport::GetMouseRay(const glm::vec2& mousePosition) const
{
	const std::shared_ptr<Entity> camera = m_Camera.lock();
	if (!camera)
	{
		return {};
	}

	glm::vec2 ndc = mousePosition / (glm::vec2)m_Size;
	ndc *= 2.0f;
	ndc -= 1.0f;
	ndc = glm::clamp(ndc, glm::vec2(-1.0, -1.0), glm::vec2(1.0, 1.0));
	const glm::vec4 rayClip = { ndc.x, -ndc.y, 0.0f, 1.0f };
	glm::vec4 rayEye = glm::inverse(m_Projection) * rayClip;
	rayEye.z = -1.0f; rayEye.w = 0.0f;
	glm::vec3 ray = (glm::vec3)(glm::inverse(camera->GetComponent<Camera>().GetViewMat4()) * rayEye);
	return glm::normalize(ray);
}

void Viewport::SetDrawGizmosCallback(const std::function<void(const glm::vec2&, glm::ivec2, std::shared_ptr<Entity>, bool&)>& drawGizmosCallback)
{
	m_DrawGizmosCallback = drawGizmosCallback;
}

void Viewport::Resize(const glm::ivec2& size)
{
	m_Size = size;

	if (const std::shared_ptr<Entity> camera = m_Camera.lock())
	{
		camera->GetComponent<Camera>().ResizeRenderView(m_Name, m_Size);
	}
	
	std::shared_ptr<ResizeEvent> event = std::make_shared<ResizeEvent>(size, m_Name, Event::Type::OnResize, this);
	EventSystem::GetInstance().SendEvent(event);
}

void Viewport::UpdateProjectionMat4()
{
	const std::shared_ptr<Entity> camera = m_Camera.lock();
	if (!camera || !camera->HasComponent<Camera>())
	{
		return;
	}

	const Camera& cameraComponenet = camera->GetComponent<Camera>();

	switch (cameraComponenet.GetType())
	{
	case Camera::Type::PERSPECTIVE:
	{
		SetPerspective(m_Size, cameraComponenet.GetZFar(), cameraComponenet.GetZNear(), cameraComponenet.GetFov());
		break;
	}
	case Camera::Type::ORTHOGRAPHIC:
	default:
	{
		SetOrthographic(m_Size, cameraComponenet.GetZFar(), cameraComponenet.GetZNear());
		break;
	}
	}
}

void Viewport::SetOrthographic(const glm::ivec2& size, const float zFar, const float zNear)
{
	if (size.x == 0 || size.y == 0)
	{
		m_Size = { 1, 1 };
	}
	else
	{
		m_Size = size;
	}

	const float ratio = static_cast<float>(size.x) / static_cast<float>(size.y);
	m_Projection = glm::ortho(-ratio * 10.0f, ratio * 10.0f, -1.0f * 10.0f,
		1.0f * 10.0f, zFar, zNear);
}

void Viewport::SetPerspective(const glm::ivec2& size, const float zFar, const float zNear, const float fov)
{
	if (size.x == 0 || size.y == 0)
	{
		m_Size = { 1, 1 };
	}
	else
	{
		m_Size = size;
	}

	const float ratio = static_cast<float>(size.x) / static_cast<float>(size.y);
	m_Projection = glm::perspectiveRH_ZO(fov, ratio, zFar, zNear);
}
