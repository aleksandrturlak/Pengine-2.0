#include "Editor.h"

#include "Core/AsyncAssetLoader.h"
#include "Core/BindlessUniformWriter.h"
#include "Core/FileFormatNames.h"
#include "Core/Input.h"
#include "Core/KeyCode.h"
#include "Core/SceneManager.h"
#include "Core/MaterialManager.h"
#include "Core/MeshManager.h"
#include "Core/Serializer.h"
#include "Core/TextureManager.h"
#include "Core/ThreadPool.h"
#include "Core/Time.h"
#include "Core/ViewportManager.h"
#include "Core/Viewport.h"
#include "Core/WindowManager.h"
#include "Core/RenderPassManager.h"
#include "Core/RenderPassOrder.h"
#include "Core/ClayManager.h"
#include "Core/Profiler.h"
#include "Core/ReflectionSystem.h"

#include "EventSystem/EventSystem.h"
#include "EventSystem/NextFrameEvent.h"

#include "Graphics/Device.h"
#include "Graphics/Renderer.h"
#include "Graphics/BaseMaterial.h"

#include "ImGuizmo.h"

#include "Components/Camera.h"
#include "Components/Decal.h"
#include "Components/DirectionalLight.h"
#include "Components/PointLight.h"
#include "Components/SpotLight.h"
#include "Components/Renderer3D.h"
#include "Components/SkeletalAnimator.h"
#include "Components/EntityAnimator.h"
#include "Components/Transform.h"
#include "Components/Canvas.h"
#include "Components/RigidBody.h"

#include "ComponentSystems/PhysicsSystem.h"

#include <fstream>
#include <format>

using namespace Pengine;

Editor::Editor()
{
	SetDarkThemeColors();

	m_AssetBrowserFilterBuffer[0] = '\0';

	m_Thumbnails.Initialize();

	SceneManager::GetInstance().SetIsPhysicsSystemsUpdating(false);
}

Editor::~Editor()
{
	m_Thumbnails.ShutDown();
}

void Editor::Update(const std::shared_ptr<Scene>& scene, Window& window)
{
	PROFILER_SCOPE(__FUNCTION__);

	Input& input = Input::GetInstance(&window);

	if (input.IsMouseReleased(KeyCode::MOUSE_BUTTON_2))
	{
		m_MovingCamera = nullptr;
	}

	for (const auto& [name, viewport] : window.GetViewportManager().GetViewports())
	{
		if (viewport->IsHovered() && input.IsMouseDown(KeyCode::MOUSE_BUTTON_2))
		{
			m_MovingCamera = viewport->GetCamera().lock();
		}

		if (m_MovingCamera)
		{
			WindowManager::GetInstance().GetCurrentWindow()->DisableCursor();
			MoveCamera(m_MovingCamera, window);
			break;
		}
		else
		{
			WindowManager::GetInstance().GetCurrentWindow()->ShowCursor();
		}
	}

	if (input.IsKeyDown(KeyCode::KEY_LEFT_CONTROL) && input.IsKeyPressed(KeyCode::KEY_F))
	{
		m_FullScreen = !m_FullScreen;
	}

	if (m_FullScreen)
	{
		return;
	}

	Manipulate(scene, window);

	MainMenuBar(scene);
	Hierarchy(scene, window);
	SceneInfo(scene);
	Properties(scene, window);
	AssetBrowser(scene);
	AssetBrowserHierarchy();
	PlayButtonMenu(scene);

	m_MeshMenu.Update(*this);
	m_MaterialMenu.Update(*this);
	m_BaseMaterialMenu.Update(*this);
	m_CreateFileMenu.Update();
	m_DeleteFileMenu.Update();
	m_CloneMaterialMenu.Update();
	m_CreateViewportMenu.Update(*this, window);
	m_LoadIntermediateMenu.Update();
	m_TextureMetaPropertiesMenu.Update();
	m_ImportMenu.Update(*this);
	m_EntityAnimatorEditor.Update(this);

	m_Thumbnails.UpdateThumbnails();

	const auto& globalDataAccessor = GlobalDataAccessor::GetInstance();

	ImGui::Begin("Settings");
	ImGui::Text("FPS: %.0f", 1.0f / static_cast<float>(Time::GetDeltaTime()));
	ImGui::Text("DrawCalls: %d", globalDataAccessor.GetDrawCallCount());
	ImGui::Text("Triangles: %d", static_cast<int>(globalDataAccessor.GetTriangleCount()));
	ImGui::Text("Meshes: %d", static_cast<int>(MeshManager::GetInstance().GetMeshes().size()));
	ImGui::Text("BaseMaterials: %d", static_cast<int>(MaterialManager::GetInstance().GetBaseMaterials().size()));
	ImGui::Text("Materials: %d", static_cast<int>(MaterialManager::GetInstance().GetMaterials().size()));
	ImGui::Text("Textures: %d", static_cast<int>(TextureManager::GetInstance().GetTextures().size()));
	ImGui::Text("VRAM Allocated: %.3f GB", static_cast<float>(globalDataAccessor.GetVramAllocated() / 1024.0f / 1024.0f / 1024.0f));

	ImGui::Checkbox("Snap", &isSnapEnabled);
	if (isSnapEnabled)
	{
		ImGui::SliderFloat("Snap Value", &snap, 0.00001f, 1.0f);
	}

	bool isPhysicsSystemsUpdating = SceneManager::GetInstance().IsPhysicsSystemsUpdating();
	if (ImGui::Checkbox("Simulate Physics", &isPhysicsSystemsUpdating))
	{
		SceneManager::GetInstance().SetIsPhysicsSystemsUpdating(isPhysicsSystemsUpdating);
	}

	ImGui::End();
}

bool Editor::DrawAngle3Control(
	const std::string& label,
	glm::vec3& values,
	float resetValue,
	const glm::vec2& limits,
	float columnWidth) const
{
	bool changed = false;

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	if (m_DrawVecLabel) ImGui::Text(label.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 5.0f });

	const float lineHeight = ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f;
	const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
	{
		values.x = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::SliderAngle("##X", &values.x, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
	{
		values.y = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::SliderAngle("##Y", &values.y, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	if (ImGui::Button("Z", buttonSize))
	{
		values.z = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::SliderAngle("##Z", &values.z, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();

	return changed;
}

bool Editor::DrawVec2Control(
	const std::string& label,
	glm::vec2& values,
	const float resetValue,
	const glm::vec2& limits,
	const float speed,
	const float columnWidth) const
{
	bool changed = false;

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	if (m_DrawVecLabel) ImGui::Text(label.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 5.0f });

	const float lineHeight = ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f;
	const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
	{
		values.x = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##X", &values.x, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
	{
		values.y = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##Y", &values.y, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();

	return changed;
}

bool Editor::DrawVec3Control(
	const std::string& label,
	glm::vec3& values,
	const float resetValue,
	const glm::vec2& limits,
	const float speed,
	const float columnWidth) const
{
	bool changed = false;

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	if (m_DrawVecLabel) ImGui::Text(label.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 5.0f });

	const float lineHeight = ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f;
	const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
	{
		values.x = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##X", &values.x, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
	{
		values.y = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##Y", &values.y, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	if (ImGui::Button("Z", buttonSize))
	{
		values.z = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##Z", &values.z, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();

	return changed;
}

bool Editor::DrawVec4Control(
	const std::string& label,
	glm::vec4& values,
	const float resetValue,
	const glm::vec2& limits,
	const float speed,
	const float columnWidth) const
{
	bool changed = false;

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	if (m_DrawVecLabel) ImGui::Text(label.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 5.0f });

	const float lineHeight = ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f;
	const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
	{
		values.x = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##X", &values.x, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
	{
		values.y = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##Y", &values.y, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	if (ImGui::Button("Z", buttonSize))
	{
		values.z = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##Z", &values.z, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.9f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
	if (ImGui::Button("W", buttonSize))
	{
		values.w = resetValue;
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragFloat("##W", &values.w, speed, limits.x, limits.y, "%.2f"))
	{
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();

	return changed;
}

bool Editor::DrawIVec2Control(
	const std::string& label,
	glm::ivec2& values,
	const float resetValue,
	const glm::vec2& limits,
	const float speed,
	const float columnWidth) const
{
	bool changed = false;

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	if (m_DrawVecLabel) ImGui::Text(label.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 5.0f });

	const float lineHeight = ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f;
	const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
	{
		values.x = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##X", &values.x, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
	{
		values.y = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##Y", &values.y, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();

	return changed;
}

bool Editor::DrawIVec3Control(
	const std::string& label,
	glm::ivec3& values,
	const float resetValue,
	const glm::vec2& limits,
	const float speed,
	const float columnWidth) const
{
	bool changed = false;

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	if (m_DrawVecLabel) ImGui::Text(label.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 5.0f });

	const float lineHeight = ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f;
	const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
	{
		values.x = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##X", &values.x, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
	{
		values.y = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##Y", &values.y, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	if (ImGui::Button("Z", buttonSize))
	{
		values.z = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##Z", &values.z, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();

	return changed;
}

bool Editor::DrawIVec4Control(
	const std::string& label,
	glm::ivec4& values,
	const float resetValue,
	const glm::vec2& limits,
	const float speed,
	const float columnWidth) const
{
	bool changed = false;

	ImGuiIO& io = ImGui::GetIO();

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	if (m_DrawVecLabel) ImGui::Text(label.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 5.0f });

	const float lineHeight = ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f;
	const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
	{
		values.x = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##X", &values.x, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
	{
		values.y = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##Y", &values.y, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	if (ImGui::Button("Z", buttonSize))
	{
		values.z = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##Z", &values.z, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.9f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
	if (ImGui::Button("W", buttonSize))
	{
		values.w = static_cast<int>(resetValue);
		changed = true;
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	if (ImGui::DragInt("##W", &values.w, speed, limits.x, limits.y))
	{
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();

	return changed;
}

bool Editor::ImageCheckBox(const void* id, ImTextureID textureOn, ImTextureID textureOff, bool& enabled)
{
	const ImTextureID isEnabledIcon = enabled ? textureOn : textureOff;

	bool clicked = false;

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushID(id);
	if (ImGui::ImageButton(isEnabledIcon, { ImGui::GetFontSize(), ImGui::GetFontSize() }))
	{
		clicked = true;
		enabled = !enabled;
	}
	ImGui::PopID();
	ImGui::PopStyleColor();

	return clicked;
}

void Editor::Hierarchy(const std::shared_ptr<Scene>& scene, Window& window)
{
	if (ImGui::Begin("Scene Hierarchy"))
	{
		Input& input = Input::GetInstance(&window);

		if (ImGui::IsWindowFocused() && input.IsKeyDown(KeyCode::KEY_LEFT_CONTROL) && input.IsKeyPressed(KeyCode::KEY_A))
		{
			scene->GetSelectedEntities().clear();
			for (const std::shared_ptr<Entity> entity : scene->GetEntities())
			{
				scene->GetSelectedEntities().emplace(entity);
			}
		}

		if (scene)
		{
			DrawScene(scene, window);

			GameObjectPopUpMenu(scene);
		}

		ImGui::End();
	}
}

void Editor::SceneInfo(const std::shared_ptr<Scene>& scene)
{
	if (ImGui::Begin("Scene"))
	{
		if (scene)
		{
			// Generate thumbnail for the scene if there is no one.
			m_Thumbnails.GetOrGenerateThumbnail(m_RootDirectory / scene->GetFilepath(), scene, Thumbnails::Type::SCENE);

			ImGui::Text("Name: %s", scene->GetName().c_str());
			ImGui::Text("Filepath: %s", scene->GetFilepath().string().c_str());
			ImGui::Text("Tag: %s", scene->GetTag().c_str());
			ImGui::Text("Entities Count: %zu", scene->GetEntities().size());

			bool drawBoundingBoxes = scene->GetSettings().drawBoundingBoxes;
			if (ImGui::Checkbox("Draw Bounding Boxes", &drawBoundingBoxes))
			{
				scene->GetSettings().drawBoundingBoxes = drawBoundingBoxes;
			}

			bool drawPhysicsShapes = scene->GetSettings().drawPhysicsShapes;
			if (ImGui::Checkbox("Draw Physics Shapes", &drawPhysicsShapes))
			{
				scene->GetSettings().drawPhysicsShapes = drawPhysicsShapes;
			}

			if (ImGui::CollapsingHeader("Wind Settings"))
			{
				Indent indent;
				Scene::WindSettings windSettings = scene->GetWindSettings();
				DrawVec3Control("Wind Direction", windSettings.direction, 0.0f, { -1.0f, 1.0f });
				ImGui::SliderFloat("Strength##Wind", &windSettings.strength, 0.0f, 10.0f);
				ImGui::SliderFloat("Frequency##Wind ", &windSettings.frequency, 0.0f, 10.0f);
				scene->SetWindSettings(windSettings);
			}

			GraphicsSettingsInfo(scene->GetGraphicsSettings());
		}

		ImGui::End();
	}
}

void Editor::DrawScene(const std::shared_ptr<Scene>& scene, Window& window)
{
	Indent indent;

	if (ImGui::TreeNodeEx((void*)&scene, 0, scene->GetName().c_str()))
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT"))
			{
				UUID* uuidPtr = (UUID*)payload->Data;
				auto callback = [weakScene = std::weak_ptr<Scene>(scene), uuid = *uuidPtr]()
				{
					if (const std::shared_ptr<Scene> currentScene = weakScene.lock())
					{
						if (const std::shared_ptr<Entity> entity = currentScene->FindEntityByUUID(uuid); entity)
						{
							if (const std::shared_ptr<Entity> parent = entity->GetParent())
							{
								parent->RemoveChild(entity);
							}
						}
					}
				};

				std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
				EventSystem::GetInstance().SendEvent(event);
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::TreePop();

		const std::vector<std::shared_ptr<Entity>>& entities = scene->GetEntities();
		for (const std::shared_ptr<Entity>& entity : entities)
		{
			if (entity->HasParent())
			{
				continue;
			}

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
			flags |= scene->GetSelectedEntities().count(entity) ?
				ImGuiTreeNodeFlags_Selected : 0;
			
			DrawNode(entity, flags, window);
		}
	}
}

void Editor::DrawNode(const std::shared_ptr<Entity>& entity, ImGuiTreeNodeFlags flags, Window& window)
{
	flags |= entity->GetChilds().empty() ? ImGuiTreeNodeFlags_Leaf : 0;

	Indent indent;

	bool enabled = entity->IsEnabled();

	const ImTextureID showIconId = (ImTextureID)TextureManager::GetInstance().GetTexture(std::filesystem::path("Editor") / "Images" / "ShowIcon.png")->GetId();
	const ImTextureID hideIconId = (ImTextureID)TextureManager::GetInstance().GetTexture(std::filesystem::path("Editor") / "Images" / "HideIcon.png")->GetId();

	if (ImageCheckBox(entity.get(), showIconId, hideIconId, enabled))
	{
		entity->SetEnabled(enabled);
	}

	ImGui::SameLine();

	ImGuiStyle* style = &ImGui::GetStyle();
	if (entity->IsPrefab())
	{
		style->Colors[ImGuiCol_Text] = ImVec4(0.557f, 0.792f, 0.902f, 1.0f);
	}

	const bool opened = ImGui::TreeNodeEx((void*)entity.get(), flags, entity->GetName().c_str());
	style->Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("GAMEOBJECT", (const void*)&entity->GetUUID(), sizeof(UUID));
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT"))
		{
			UUID* uuidPtr = (UUID*)payload->Data;
			auto callback = [weakEntity = std::weak_ptr<Entity>(entity), uuid = *uuidPtr]()
			{
				if (const std::shared_ptr<Entity>& currentEntity = weakEntity.lock())
				{
					const std::shared_ptr<Entity>& child = currentEntity->GetScene()->FindEntityByUUID(uuid);
					if (child)
					{
						if (currentEntity->HasAsParent(child, true) ||
							(currentEntity->HasParent() && currentEntity->GetParent() == child))
						{
							return;
						}

						if (const std::shared_ptr<Entity>& parent = child->GetParent())
						{
							parent->RemoveChild(child);
						}

						currentEntity->AddChild(child);
					}
				}
			};

			std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
			EventSystem::GetInstance().SendEvent(event);
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None)
		&& (ImGui::GetMousePos().x - ImGui::GetItemRectMin().x) > ImGui::GetTreeNodeToLabelSpacing())
	{
		auto& selectedEntities = entity->GetScene()->GetSelectedEntities();
		if (!Input::GetInstance(&window).IsKeyDown(KeyCode::KEY_LEFT_CONTROL))
		{
			selectedEntities.clear();
		}
		
		if (selectedEntities.contains(entity))
		{
			selectedEntities.erase(entity);
		}
		else
		{
			selectedEntities.emplace(entity);
		}
	}

	if (opened)
	{
		ImGui::TreePop();
		DrawChilds(entity, window);
	}
}

void Editor::DrawChilds(const std::shared_ptr<Entity>& entity, Window& window)
{
	for (const std::weak_ptr<Entity> weakChild : entity->GetChilds())
	{
		if (const std::shared_ptr<Entity> child = weakChild.lock())
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
			flags |= entity->GetScene()->GetSelectedEntities().count(child) ?
				ImGuiTreeNodeFlags_Selected : 0;

			DrawNode(child, flags, window);
		}
	}
}

void Editor::Properties(const std::shared_ptr<Scene>& scene, Window& window)
{
	if (ImGui::Begin("Properties", nullptr))
	{
		if (!scene)
		{
			ImGui::End();
			return;
		}

		for (const std::shared_ptr<Entity> entity : scene->GetSelectedEntities())
		{
			if (entity->HasParent())
			{
				ImGui::Text("Owner: %s", entity->GetParent()->GetName().c_str());
			}
			else
			{
				ImGui::Text("Owner: Null");
			}

			ImGui::Text("UUID: %s", entity->GetUUID().ToString().c_str());

			char name[256];
			strcpy(name, entity->GetName().c_str());
			if (ImGui::InputText("Name", name, sizeof(name)))
			{
				entity->SetName(name);
			}

			if (ImGui::CollapsingHeader("Prefab"))
			{
				bool validUUID = false;
				if (entity->IsPrefab())
				{
					ImGui::PushID("Deattach Prefab");
					if (ImGui::Button("X"))
					{
						entity->SetPrefabFilepathUUID(UUID(0, 0));
					}
					ImGui::PopID();

					ImGui::SameLine();
					
					const std::filesystem::path prefabFilepath = Utils::FindFilepath(entity->GetPrefabFilepathUUID());
					if (std::filesystem::exists(prefabFilepath))
					{
						ImGui::Text("Filepath: %s", prefabFilepath.string().c_str());
					
						validUUID = true;

						ImGui::PushID("Save Prefab");
						if (ImGui::Button("Save"))
						{
							Serializer::SerializePrefab(prefabFilepath, entity);
							
							auto callback = [scene, entity]()
							{
								std::vector<std::shared_ptr<Entity>> entitiesToDelete;
								for (const auto& maybePrefabEntity : scene->GetEntities())
								{
									if (maybePrefabEntity != entity && maybePrefabEntity->GetPrefabFilepathUUID() == entity->GetPrefabFilepathUUID())
									{
										entitiesToDelete.emplace_back(maybePrefabEntity);
									}
								}

								for (auto& entityToDelete : entitiesToDelete)
								{
									std::shared_ptr<Entity> clonedEntity = scene->CloneEntity(entity);

									if (clonedEntity->HasParent())
									{
										clonedEntity->GetParent()->RemoveChild(clonedEntity);
									}

									if (entityToDelete->HasParent())
									{
										entityToDelete->GetParent()->AddChild(clonedEntity);
									}

									clonedEntity->GetComponent<Transform>().Copy(entityToDelete->GetComponent<Transform>());
								}

								for (auto& entityToDelete : entitiesToDelete)
								{
									scene->DeleteEntity(entityToDelete);
								}
							};

							std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
							EventSystem::GetInstance().SendEvent(event);
						}
						ImGui::PopID();
					}
					else
					{
						ImGuiStyle* style = &ImGui::GetStyle();
						auto previousColor = style->Colors[ImGuiCol_Text];
						style->Colors[ImGuiCol_Text] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
						ImGui::Text("Filepath: prefab uuid is invalid!");
						style->Colors[ImGuiCol_Text] = previousColor;
					}
				}
				else
				{
					ImGui::PushID("Save Prefab");
					if (ImGui::Button("Save as prefab"))
					{
						const std::filesystem::path prefabFilepath = Utils::GetShortFilepath(m_CurrentDirectory / (entity->GetName() + FileFormats::Prefab()));
						entity->SetPrefabFilepathUUID(Serializer::GenerateFileUUID(prefabFilepath));
						Serializer::SerializePrefab(prefabFilepath, entity);
					}
					ImGui::PopID();
				}
			}

			ComponentsPopUpMenu(entity);

			TransformComponent(entity);
			CameraComponent(entity, window);
			Renderer3DComponent(entity);
			DecalComponent(entity);
			PointLightComponent(entity);
			SpotLightComponent(entity);
			DirectionalLightComponent(entity);
			SkeletalAnimatorComponent(entity);
			EntityAnimatorComponent(entity);
			CanvasComponent(entity);
			PhysicsBoxComponent(entity);
			UserComponents(entity);

			ImGui::NewLine();
		}
		ImGui::End();
	}
}

void Editor::GraphicsSettingsInfo(GraphicsSettings& graphicsSettings)
{
	if (ImGui::CollapsingHeader("Graphics settings"))
	{
		Indent indent;

		ImGui::Text("Filepath: %s", graphicsSettings.GetFilepath().string().c_str());

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));
				graphicsSettings = Serializer::DeserializeGraphicsSettings(filepath);
			}
			ImGui::EndDragDropTarget();
		}

		bool isChangedToSerialize = false;
		if (ImGui::CollapsingHeader("SSAO"))
		{
			ImGui::PushID("SSAO Is Enabled");
			isChangedToSerialize += ImGui::Checkbox("Is Enabled", &graphicsSettings.ssao.isEnabled);
			ImGui::PopID();

			const char* const resolutionScales[] = { "0.25", "0.5", "0.75", "1.0"};
			ImGui::PushID("SSAO Quality");
			isChangedToSerialize += ImGui::Combo("Quality", &graphicsSettings.ssao.resolutionScale, resolutionScales, 4);
			ImGui::PopID();

			ImGui::PushID("SSAO Blur Quality");
			isChangedToSerialize += ImGui::Combo("Blur Quality", &graphicsSettings.ssao.resolutionBlurScale, resolutionScales, 4);
			ImGui::PopID();

			ImGui::PushID("SSAO Bias");
			isChangedToSerialize += ImGui::SliderFloat("Bias", &graphicsSettings.ssao.bias, 0.0f, 10.0f);
			ImGui::PopID();
			
			ImGui::PushID("SSAO Radius");
			isChangedToSerialize += ImGui::SliderFloat("Radius", &graphicsSettings.ssao.radius, 0.0f, 1.0f);
			ImGui::PopID();
			
			ImGui::PushID("SSAO Kernel Size");
			isChangedToSerialize += ImGui::SliderInt("Kernel Size", &graphicsSettings.ssao.kernelSize, 2, 64);
			ImGui::PopID();
			
			ImGui::PushID("SSAO Noise Size");
			isChangedToSerialize += ImGui::SliderInt("Noise Size", &graphicsSettings.ssao.noiseSize, 4, 64);
			ImGui::PopID();
			
			ImGui::PushID("SSAO AO Scale");
			isChangedToSerialize += ImGui::SliderFloat("AO Scale", &graphicsSettings.ssao.aoScale, 0.0f, 10.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Shadows"))
		{
			Indent indent;
			if (ImGui::CollapsingHeader("CSM"))
			{
				Indent indent;
				ImGui::PushID("Shadows Is Enabled");
				isChangedToSerialize += ImGui::Checkbox("Is Enabled", &graphicsSettings.shadows.csm.isEnabled);
				ImGui::PopID();

				const char* const qualities[] = { "1024", "2048", "4096" };
				ImGui::PushID("Shadows Quality");
				isChangedToSerialize += ImGui::Combo("Quality", &graphicsSettings.shadows.csm.quality, qualities, 3);
				ImGui::PopID();

				ImGui::PushID("Shadows Cascade Count");
				if (ImGui::SliderInt("Cascade Count", &graphicsSettings.shadows.csm.cascadeCount, 2, 10))
				{
					isChangedToSerialize += true;

					graphicsSettings.shadows.csm.biases.resize(graphicsSettings.shadows.csm.cascadeCount, 0.0f);
				}
				ImGui::PopID();

				ImGui::PushID("Shadows Visualize");
				ImGui::Checkbox("Visualize", &graphicsSettings.shadows.csm.visualize);
				ImGui::PopID();

				ImGui::PushID("Shadows Stabilize Cascades");
				ImGui::Checkbox("Stabilize Cascades", &graphicsSettings.shadows.csm.stabilizeCascades);
				ImGui::PopID();

				const char* const filters[] = { "None", "PCF", "PoissonDisk" };
				int currentFilter = (int)graphicsSettings.shadows.csm.filter;
				ImGui::PushID("Shadows Filters");
				if (ImGui::Combo("Filter", &currentFilter, filters, 3))
				{
					graphicsSettings.shadows.csm.filter = (GraphicsSettings::Shadows::CSM::Filter)currentFilter;
					isChangedToSerialize += 1;
				}
				ImGui::PopID();

				ImGui::PushID("Shadows Pcf Range");
				isChangedToSerialize += ImGui::SliderInt("Pcf Range", &graphicsSettings.shadows.csm.pcfRange, 1, 5);
				ImGui::PopID();

				ImGui::PushID("Shadows Split Factor");
				isChangedToSerialize += ImGui::SliderFloat("Split Factor", &graphicsSettings.shadows.csm.splitFactor, 0.0f, 1.0f);
				ImGui::PopID();

				ImGui::PushID("Shadows Max Distance");
				isChangedToSerialize += ImGui::SliderFloat("Max Distance", &graphicsSettings.shadows.csm.maxDistance, 0.0f, 1000.0f);
				ImGui::PopID();

				ImGui::PushID("Shadows Fog Factor");
				isChangedToSerialize += ImGui::SliderFloat("Fog Factor", &graphicsSettings.shadows.csm.fogFactor, 0.0f, 1.0f);
				ImGui::PopID();

				for (size_t i = 0; i < graphicsSettings.shadows.csm.biases.size(); i++)
				{
					const std::string biasName = "Bias " + std::to_string(i);
					const std::string idName = "Shadows " + biasName;
					ImGui::PushID(idName.c_str());
					isChangedToSerialize += ImGui::SliderFloat(biasName.c_str(), &graphicsSettings.shadows.csm.biases[i], 0.0f, 1.0f);
					ImGui::PopID();
				}
			}

			if (ImGui::CollapsingHeader("SSS"))
			{
				Indent indent;

				ImGui::PushID("SSS Is Enabled");
				isChangedToSerialize += ImGui::Checkbox("Is Enabled", &graphicsSettings.shadows.sss.isEnabled);
				ImGui::PopID();

				const char* const resolutionScales[] = { "0.25", "0.5", "0.75", "1.0" };
				ImGui::PushID("SSS Quality");
				isChangedToSerialize += ImGui::Combo("Quality", &graphicsSettings.shadows.sss.resolutionScale, resolutionScales, 4);
				ImGui::PopID();

				ImGui::PushID("SSS Blur Quality");
				isChangedToSerialize += ImGui::Combo("Blur Quality", &graphicsSettings.shadows.sss.resolutionBlurScale, resolutionScales, 4);
				ImGui::PopID();

				ImGui::PushID("SSS Max Steps");
				isChangedToSerialize += ImGui::SliderInt("Max Steps", &graphicsSettings.shadows.sss.maxSteps, 1, 128);
				ImGui::PopID();

				ImGui::PushID("SSS Max Ray Distance");
				isChangedToSerialize += ImGui::SliderFloat("Max Ray Distance", &graphicsSettings.shadows.sss.maxRayDistance, 0.0f, 5.0f);
				ImGui::PopID();

				ImGui::PushID("SSS Max Distance");
				isChangedToSerialize += ImGui::SliderFloat("Max Distance", &graphicsSettings.shadows.sss.maxDistance, 0.0f, 5.0f);
				ImGui::PopID();

				ImGui::PushID("SSS Min Thickness");
				isChangedToSerialize += ImGui::SliderFloat("Min Thickness", &graphicsSettings.shadows.sss.minThickness, -0.1f, 0.0f, "%.6f");
				ImGui::PopID();

				ImGui::PushID("SSS Max Thickness");
				isChangedToSerialize += ImGui::SliderFloat("Max Thickness", &graphicsSettings.shadows.sss.maxThickness, 0.0f, 0.01f, "%.6f");
				ImGui::PopID();
			}

			if (ImGui::CollapsingHeader("PointLightShadows"))
			{
				Indent indent;

				ImGui::PushID("PointLightShadows Is Enabled");
				isChangedToSerialize += ImGui::Checkbox("Is Enabled", &graphicsSettings.shadows.pointLightShadows.isEnabled);
				ImGui::PopID();

				const char* const atlasQualities[] = { "1024", "2048", "3072", "4096" };
				ImGui::PushID("PointLightShadows Atlas Quality");
				isChangedToSerialize += ImGui::Combo("Atlas Size", &graphicsSettings.shadows.pointLightShadows.atlasQuality, atlasQualities, 4);
				ImGui::PopID();

				const char* const faceQualities[] = { "128", "256", "512", "1024" };
				ImGui::PushID("PointLightShadows Face Quality");
				isChangedToSerialize += ImGui::Combo("Face Size", &graphicsSettings.shadows.pointLightShadows.faceQuality, faceQualities, 4);
				ImGui::PopID();
			}

			if (ImGui::CollapsingHeader("SpotLightShadows"))
			{
				Indent indent;

				ImGui::PushID("SpotLightShadows Is Enabled");
				isChangedToSerialize += ImGui::Checkbox("Is Enabled", &graphicsSettings.shadows.spotLightShadows.isEnabled);
				ImGui::PopID();

				const char* const atlasQualities[] = { "1024", "2048", "3072", "4096" };
				ImGui::PushID("SpotLightShadows Atlas Quality");
				isChangedToSerialize += ImGui::Combo("Atlas Size", &graphicsSettings.shadows.spotLightShadows.atlasQuality, atlasQualities, 4);
				ImGui::PopID();

				const char* const faceQualities[] = { "128", "256", "512", "1024" };
				ImGui::PushID("SpotLightShadows Face Quality");
				isChangedToSerialize += ImGui::Combo("Face Size", &graphicsSettings.shadows.spotLightShadows.faceQuality, faceQualities, 4);
				ImGui::PopID();
			}
		}

		if (ImGui::CollapsingHeader("Bloom"))
		{
			ImGui::PushID("Bloom Is Enabled");
			isChangedToSerialize += ImGui::Checkbox("Is Enabled", &graphicsSettings.bloom.isEnabled);
			ImGui::PopID();

			const char* const resolutionScales[] = { "0.25", "0.5", "0.75", "1.0" };
			ImGui::PushID("Bloom Quality");
			isChangedToSerialize += ImGui::Combo("Quality", &graphicsSettings.bloom.resolutionScale, resolutionScales, 4);
			ImGui::PopID();

			ImGui::PushID("Bloom Mip Count");
			isChangedToSerialize += ImGui::SliderInt("Mip Count", &graphicsSettings.bloom.mipCount, 1, 10);
			ImGui::PopID();

			ImGui::PushID("Bloom Intensity");
			isChangedToSerialize += ImGui::SliderFloat("Intensity", &graphicsSettings.bloom.intensity, 0.0f, 2.0f);
			ImGui::PopID();

			ImGui::PushID("Bloom Brightness Threshold");
			isChangedToSerialize += ImGui::SliderFloat("Brightness Threshold", &graphicsSettings.bloom.brightnessThreshold, 0.0f, 2.0f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("SSR"))
		{
			ImGui::PushID("SSR Is Enabled");
			isChangedToSerialize += ImGui::Checkbox("Is Enabled", &graphicsSettings.ssr.isEnabled);
			ImGui::PopID();

			const char* const resolutionScales[] = { "0.25", "0.5", "0.75", "1.0" };
			ImGui::PushID("SSR Quality");
			isChangedToSerialize += ImGui::Combo("Quality", &graphicsSettings.ssr.resolutionScale, resolutionScales, 4);
			ImGui::PopID();

			const char* const resolutionBlurScales[] = { "0.125", "0.25", "0.5", "0.75", "1.0" };
			ImGui::PushID("SSR Blur Quality");
			isChangedToSerialize += ImGui::Combo("Blur Quality", &graphicsSettings.ssr.resolutionBlurScale, resolutionBlurScales, 5);
			ImGui::PopID();

			const char* const blurTypes[] = { "None", "Simple", "Bilateral" };
			int blur = (int)graphicsSettings.ssr.blur;
			ImGui::PushID("SSR Blur Type");
			if (ImGui::Combo("Blur Type", &blur, blurTypes, 3))
			{
				graphicsSettings.ssr.blur = (GraphicsSettings::SSR::Blur)blur;
				isChangedToSerialize += 1;
			}
			ImGui::PopID();

			ImGui::PushID("SSR Max Distance");
			isChangedToSerialize += ImGui::SliderFloat("Max Distance", &graphicsSettings.ssr.maxDistance, 1.0f, 100.0f);
			ImGui::PopID();

			ImGui::PushID("SSR Resolution");
			isChangedToSerialize += ImGui::SliderFloat("Resolution", &graphicsSettings.ssr.resolution, 0.0f, 1.0f);
			ImGui::PopID();

			ImGui::PushID("SSR Step Count");
			isChangedToSerialize += ImGui::SliderInt("Step Count", &graphicsSettings.ssr.stepCount, 0, 20);
			ImGui::PopID();

			ImGui::PushID("SSR Thickness");
			isChangedToSerialize += ImGui::SliderFloat("Thickness", &graphicsSettings.ssr.thickness, 0.0f, 5.0f);
			ImGui::PopID();

			if (graphicsSettings.ssr.blur == GraphicsSettings::SSR::Blur::SIMPLE)
			{
				ImGui::PushID("SSR Blur Range");
				isChangedToSerialize += ImGui::SliderInt("Blur Range", &graphicsSettings.ssr.blurRange, 0, 10);
				ImGui::PopID();

				ImGui::PushID("SSR Blur Offset");
				isChangedToSerialize += ImGui::SliderInt("Blur Offset", &graphicsSettings.ssr.blurOffset, 0, 10);
				ImGui::PopID();
			}

			ImGui::PushID("SSR Mip Multiplier");
			isChangedToSerialize += ImGui::SliderFloat("Mip Multiplier", &graphicsSettings.ssr.mipMultiplier, 0.0f, 10.0f);
			ImGui::PopID();

			ImGui::PushID("SSR Use SkyBox Fallback");
			isChangedToSerialize += ImGui::Checkbox("Use SkyBox Fallback", &graphicsSettings.ssr.useSkyBoxFallback);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("HiZ Occlusion Culling"))
		{
			ImGui::PushID("HiZ Occlusion Culling Is Enabled");
			isChangedToSerialize += ImGui::Checkbox("Is Enabled", &graphicsSettings.hiZOcclusionCulling.isEnabled);
			ImGui::PopID();

			ImGui::PushID("HiZ Occlusion Culling Depth Bias");
			isChangedToSerialize += ImGui::SliderFloat("Depth Bias", &graphicsSettings.hiZOcclusionCulling.depthBias, 0.0f, 0.5f);
			ImGui::PopID();
		}

		if (ImGui::CollapsingHeader("Post Process"))
		{
			ImGui::PushID("Post Process FXAA");
			isChangedToSerialize += ImGui::Checkbox("FXAA", &graphicsSettings.postProcess.fxaa);
			ImGui::PopID();

			ImGui::PushID("Post Process Gamma");
			isChangedToSerialize += ImGui::SliderFloat("Gamma", &graphicsSettings.postProcess.gamma, 0.0f, 3.0f);
			ImGui::PopID();

			const char* const toneMappers[] = {"NONE", "ACES" };
			int* toneMapperIndex = (int*)(&graphicsSettings.postProcess.toneMapper);
			ImGui::PushID("Post Process Tone Mapper");
			isChangedToSerialize += ImGui::Combo("Tone Mapper", toneMapperIndex, toneMappers, (int)GraphicsSettings::PostProcess::ToneMapper::COUNT);
			ImGui::PopID();
		}

		if (isChangedToSerialize && std::filesystem::exists(graphicsSettings.GetFilepath()))
		{
			Serializer::SerializeGraphicsSettings(graphicsSettings);
		}
	}
}

void Editor::CameraComponent(const std::shared_ptr<Entity>& entity, Window& window)
{
	if (!entity->HasComponent<Camera>())
	{
		return;
	}

	Camera& camera = entity->GetComponent<Camera>();
	
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<Camera>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::SameLine();
	if (ImGui::CollapsingHeader("Camera"))
	{
		Indent indent;

		int type = static_cast<int>(camera.GetType());
		const char* types[] = { "ORTHOGRAPHIC", "PERSPECTIVE" };
		if (ImGui::Combo("Camera type", &type, types, 2))
		{
			camera.SetType(Camera::Type(type));
		}

		float fov = camera.GetFov();
		if (ImGui::SliderAngle("FOV", &fov, 0.0f, 120.0f))
		{
			camera.SetFov(fov);
		}

		float zFar = camera.GetZFar();
		if (ImGui::SliderFloat("Z Far", &zFar, 0.0f, 1000.0f))
		{
			camera.SetZFar(zFar);
		}

		float zNear = camera.GetZNear();
		if (ImGui::SliderFloat("Z Near", &zNear, 0.0f, 10.0f))
		{
			camera.SetZNear(zNear);
		}

		if (ImGui::BeginMenu("Viewports"))
		{
			for (const auto& viewport : window.GetViewportManager().GetViewports())
			{
				if (ImGui::MenuItem(viewport.first.c_str()))
				{
					auto callback = [viewport, entity]()
					{
						viewport.second->SetCamera(entity);
					};

					std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
					EventSystem::GetInstance().SendEvent(event);
				}
			}
			
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Render Targets"))
		{
			for (const auto& passName : passPerViewportOrder)
			{
				if (ImGui::MenuItem(passName.c_str()))
				{
					camera.SetPassName(passName);
					camera.SetRenderTargetIndex(0);
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Render Target Index"))
		{
			if (!camera.GetPassName().empty())
			{
				if (RenderPassManager::GetInstance().GetPass(camera.GetPassName())->GetType() == Pass::Type::GRAPHICS)
				{
					const int indexCount = RenderPassManager::GetInstance().GetRenderPass(camera.GetPassName())->GetAttachmentDescriptions().size();
					for (size_t i = 0; i < indexCount; i++)
					{
						if (ImGui::MenuItem(std::to_string(i).c_str()))
						{
							camera.SetRenderTargetIndex(i);
						}
					}
				}
			}

			ImGui::EndMenu();
		}

		auto objectVisibilityMask = camera.GetObjectVisibilityMask();
		DrawBitMask("Object Visibility Mask", &objectVisibilityMask, 8, 2);
		camera.SetObjectVisibilityMask(objectVisibilityMask);

		auto shadowVisibilityMask = camera.GetShadowVisibilityMask();
		DrawBitMask("Shadow Visibility Mask", &shadowVisibilityMask, 8, 2);
		camera.SetShadowVisibilityMask(shadowVisibilityMask);
	}
}

void Editor::TransformComponent(const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<Transform>())
	{
		return;
	}

	Transform& transform = entity->GetComponent<Transform>();

	ImGui::PushID("Transform O");
	if (ImGui::Button("O"))
	{
	}
	ImGui::PopID();
	
	ImGui::SameLine();
	
	if (ImGui::CollapsingHeader("Transform"))
	{
		Indent indent;

		ImGui::Text("Entity: %s", transform.GetEntity()->GetName().c_str());

		bool followOwner = transform.GetFollorOwner();
		if (ImGui::Checkbox("Follow owner", &followOwner))
		{
			transform.SetFollowOwner(followOwner);
		}

		const char* types[] = { "Local", "Global" };
		ImGui::Combo("System", &m_TransformSystem, types, 2);

		std::shared_ptr<Entity> parent;
		glm::vec3 position;
		glm::vec3 rotation;
		glm::vec3 scale;
		if (m_TransformSystem == 0)
		{
			position = transform.GetPosition(Transform::System::LOCAL);
			rotation = transform.GetRotation(Transform::System::LOCAL);
			scale = transform.GetScale(Transform::System::LOCAL);
		}
		else if (m_TransformSystem == 1)
		{
			if (parent = entity->GetParent())
			{
				parent->RemoveChild(entity);
			}

			position = transform.GetPosition(Transform::System::GLOBAL);
			rotation = transform.GetRotation(Transform::System::GLOBAL);
			scale = transform.GetScale(Transform::System::GLOBAL);
		}

		DrawVec3Control("Translation", position);
		DrawAngle3Control("Rotation", rotation, 0.0f, { -360.0f, 360.0f });
		DrawVec3Control("Scale", scale, 1.0f, { 0.0f, 25.0f });
		transform.Scale(scale);
		transform.Translate(position);
		transform.Rotate(rotation);

		if (parent)
		{
			parent->AddChild(entity);
		}

		if (ImGui::Button("Copy"))
		{
			m_CopyTransform.position = position;
			m_CopyTransform.rotation = rotation;
			m_CopyTransform.scale = scale;
		}

		ImGui::SameLine();

		if (ImGui::Button("Paste"))
		{
			transform.Translate(m_CopyTransform.position);
			transform.Rotate(m_CopyTransform.rotation);
			transform.Scale(m_CopyTransform.scale);
		}
	}
}

void Editor::GameObjectPopUpMenu(const std::shared_ptr<Scene>& scene)
{
	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::MenuItem("Create Gameobject"))
		{
			const std::shared_ptr<Entity> entity = scene->CreateEntity();
			entity->AddComponent<Transform>(entity);

			scene->GetSelectedEntities().clear();
			scene->GetSelectedEntities().emplace(entity);
		}

		if (ImGui::MenuItem("Clone Gameobject"))
		{
			std::set<std::shared_ptr<Entity>> clonedEntities;
			for (std::shared_ptr<Entity> entity : scene->GetSelectedEntities())
			{
				clonedEntities.emplace(scene->CloneEntity(entity));
			}

			scene->GetSelectedEntities().clear();
			scene->GetSelectedEntities() = std::move(clonedEntities);
		}

		if (!scene->GetSelectedEntities().empty())
		{
			if (ImGui::MenuItem("Delete Selected"))
			{
				auto callback = [weakScene = std::weak_ptr<Scene>(scene)]()
				{
					if (const std::shared_ptr<Scene>& currentScene = weakScene.lock())
					{
						for (std::shared_ptr<Entity> entity : currentScene->GetSelectedEntities())
						{
							if (entity)
							{
								currentScene->DeleteEntity(entity);
							}
						}
						currentScene->GetSelectedEntities().clear();
					}
				};

				std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
				EventSystem::GetInstance().SendEvent(event);
			}
		}

		if (ImGui::MenuItem("Save Scene"))
		{
			SaveScene(scene);
		}
		
		ImGui::EndPopup();
	}
}

void Editor::AssetBrowser(const std::shared_ptr<Scene>& scene)
{
	if (ImGui::Begin("Asset browser"))
	{
		if (m_CurrentDirectory != m_RootDirectory)
		{
			if (ImGui::Button("<-"))
			{
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}
		}
		else
		{
			ImGui::Button("<-");
		}

		ImGui::SameLine();

		ImGui::PushItemWidth(32 * ImGui::GetFontSize());
		ImGui::InputTextWithHint("##AssetBrowserFilter", "Search", m_AssetBrowserFilterBuffer, 64);
		ImGui::PopItemWidth();

		{
			std::vector<std::filesystem::path> paths;
			std::filesystem::path currentPath = m_CurrentDirectory;
			paths.emplace_back(currentPath);

			while (currentPath != m_RootDirectory)
			{
				currentPath = currentPath.parent_path();
				paths.emplace_back(currentPath);
			}

			for (auto path = paths.rbegin(); path != paths.rend(); path++)
			{
				ImGui::SameLine();

				if (ImGui::Button(path->filename().string().c_str()))
				{
					m_CurrentDirectory = *path;
				}

				ImGui::SameLine();

				ImGui::Text("/");
			}
		}

		const float padding = 16.0f;
		const float thumbnailSize = 128.0f * m_ThumbnailScale;
		const float cellSize = padding + thumbnailSize;
		const float panelWidth = ImGui::GetContentRegionAvail().x;

		const int columns = (int)(panelWidth / cellSize);

		if (columns == 0)
		{
			ImGui::End();
			return;
		}

		ImGui::Columns(columns, 0, false);

		const bool leftMouseButtonDoubleClicked = ImGui::IsMouseDoubleClicked(0);

		bool iconHovered = false;

		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::MenuItem("Create Scene"))
			{
				m_CreateFileMenu.opened = true;
				m_CreateFileMenu.filepath = m_CurrentDirectory;
				m_CreateFileMenu.format = FileFormats::Scene();
				m_CreateFileMenu.name[0] = '\0';
			}

			ImGui::EndPopup();
		}

		for (auto& directoryIter : std::filesystem::directory_iterator(m_CurrentDirectory))
		{
			const std::filesystem::path path = Utils::GetShortFilepath(directoryIter.path());
			const std::string filename = path.filename().string();
			const std::string format = Utils::GetFileFormat(path);

			if (format == ".cpp"
				|| format == ".h"
				|| format == FileFormats::Meta())
			{
				continue;
			}
			
			if (FileFormats::IsAsset(format))
			{
				std::filesystem::path metaFilePath = path;
				metaFilePath.concat(FileFormats::Meta());
				if (!std::filesystem::exists(metaFilePath))
				{
					Serializer::GenerateFileUUID(path);
				}
			}

			if (m_AssetBrowserFilterBuffer[0] != '\0' && !Utils::Contains(Utils::ToLower(filename), m_AssetBrowserFilterBuffer))
			{
				continue;
			}

			ThumbnailAtlas::TileInfo fileIconInfo = GetFileIcon(directoryIter.path(), format);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushID(filename.c_str());
			if (ImGui::ImageButton(
				fileIconInfo.atlasId,
				{ thumbnailSize, thumbnailSize },
				ImVec2(fileIconInfo.uvStart.x, fileIconInfo.uvStart.y),
				ImVec2(fileIconInfo.uvEnd.x, fileIconInfo.uvEnd.y)))
			{

			}
			ImGui::PopID();

			iconHovered = iconHovered ? iconHovered : ImGui::IsItemHovered();

			if (ImGui::BeginDragDropSource())
			{
				std::wstring dragDropSource = path.wstring();
				const wchar_t* assetPath = dragDropSource.data();
				const size_t size = wcslen(assetPath);
				ImGui::SetDragDropPayload("ASSETS_BROWSER_ITEM", assetPath, size * sizeof(wchar_t));
				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor();

			if (ImGui::BeginPopupContextItem())
			{
				if (FileFormats::IsMeshIntermediate(format))
				{
					if (ImGui::MenuItem("Import"))
					{
						m_ImportMenu.opened = true;
						m_ImportMenu.importInfo = Serializer::GetImportInfo(path);
					}
				}
				if (format == FileFormats::Scene())
				{
					if (ImGui::MenuItem("Load Scene"))
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
				}
				else if (format == FileFormats::BaseMat())
				{
					if (ImGui::MenuItem("Load"))
					{
						m_BaseMaterialMenu.opened = true;
						m_BaseMaterialMenu.baseMaterial = MaterialManager::GetInstance().LoadBaseMaterial(path);
					}
				}
				if (format == FileFormats::Mat())
				{
					if (ImGui::MenuItem("Load"))
					{
						m_MaterialMenu.opened = true;
						m_MaterialMenu.material = MaterialManager::GetInstance().LoadMaterial(path);
					}
					if (ImGui::MenuItem("Clone"))
					{
						m_CloneMaterialMenu.opened = true;
						m_CloneMaterialMenu.material = MaterialManager::GetInstance().LoadMaterial(path);
						m_CloneMaterialMenu.name[0] = '\0';
					}
				}
				else if (format == FileFormats::Mesh())
				{
					if (ImGui::MenuItem("Load"))
					{
						m_MeshMenu.opened = true;
						m_MeshMenu.mesh = MeshManager::GetInstance().LoadMesh(path);
					}
				}
				if (FileFormats::IsTexture(format))
				{
					if (ImGui::MenuItem("Meta"))
					{
						m_TextureMetaPropertiesMenu.opened = true;
						m_TextureMetaPropertiesMenu.meta = *Serializer::DeserializeTextureMeta(path.string() + FileFormats::Meta());
					}
				}
				if (ImGui::MenuItem("Delete file"))
				{
					m_DeleteFileMenu.opened = true;
					m_DeleteFileMenu.filepath = path;
				}

				ImGui::EndPopup();
			}

			if (ImGui::IsItemHovered() && leftMouseButtonDoubleClicked)
			{
				if (directoryIter.is_directory())
				{
					m_CurrentDirectory /= directoryIter.path().filename();
				}
			}
			ImGui::TextWrapped("%s", filename.c_str());

			ImGui::NextColumn();
		}
		ImGui::Columns(1);

		ImGui::End();
	}
}

void Editor::AssetBrowserHierarchy()
{
	if (ImGui::Begin("Asset Browser Hierarchy"))
	{
		DrawAssetBrowserHierarchy(m_RootDirectory);

		ImGui::End();
	}
}

void Editor::DrawAssetBrowserHierarchy(const std::filesystem::path& directory)
{
	Indent indent;

	for (auto& directoryIter : std::filesystem::directory_iterator(directory))
	{
		const std::filesystem::path path = Utils::GetShortFilepath(directoryIter.path());
		if (Utils::Contains(path.string(), ".cpp") || Utils::Contains(path.string(), ".h"))
		{
			continue;
		}

		const bool isDirectory = std::filesystem::is_directory(path);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
		if (!isDirectory)
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		ThumbnailAtlas::TileInfo fileIconInfo = GetFileIcon(path, Utils::GetFileFormat(path));
		ImGui::Image(
			fileIconInfo.atlasId,
			ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize()),
			ImVec2(fileIconInfo.uvStart.x, fileIconInfo.uvStart.y),
			ImVec2(fileIconInfo.uvEnd.x, fileIconInfo.uvEnd.y));
		ImGui::SameLine();

		const std::string filename = path.filename().string();
		const bool opened = ImGui::TreeNodeEx(filename.c_str(), flags);

		if (isDirectory)
		{
			if (ImGui::IsItemClicked() && (ImGui::GetMousePos().x - ImGui::GetItemRectMin().x) > ImGui::GetTreeNodeToLabelSpacing())
			{
				m_CurrentDirectory = directoryIter.path();
			}
		}

		if (opened)
		{
			ImGui::TreePop();

			if (isDirectory)
			{
				DrawAssetBrowserHierarchy(directoryIter.path());
			}
		}
	}
}

void Editor::SetDarkThemeColors()
{
	auto& colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

	// Headers
	colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
	colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
	colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

	// Buttons
	colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
	colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
	colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

	// Frame BG
	colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
	colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
	colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

	// Tabs
	colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
	colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
	colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

	// Title
	colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
}

void Editor::Manipulate(const std::shared_ptr<Scene>& scene, Window& window)
{
	if (!scene)
	{
		return;
	}

	if (scene->GetSelectedEntities().empty())
	{
		return;
	}

	for (const auto& [name, viewport] : window.GetViewportManager().GetViewports())
	{
		if (!viewport || !viewport->GetCamera().lock())
		{
			continue;
		}

		auto callback = [this, &window, viewport](
			const glm::vec2& position,
			const glm::ivec2 size,
			const std::shared_ptr<Entity>& camera,
			bool& active)
		{
			if (!viewport || !camera)
			{
				return;
			}

			Input& input = Input::GetInstance(&window);

			if (viewport->IsHovered() && !input.IsMouseDown(KeyCode::MOUSE_BUTTON_2))
			{
				if (input.IsKeyPressed(KeyCode::KEY_W))
				{
					viewport->GetGizmoOperation() = ImGuizmo::OPERATION::TRANSLATE;
				}
				else if (input.IsKeyPressed(KeyCode::KEY_R))
				{
					viewport->GetGizmoOperation() = ImGuizmo::OPERATION::ROTATE;
				}
				else if (input.IsKeyPressed(KeyCode::KEY_S))
				{
					viewport->GetGizmoOperation() = ImGuizmo::OPERATION::SCALE;
				}
				else if (input.IsKeyPressed(KeyCode::KEY_U))
				{
					viewport->GetGizmoOperation() = ImGuizmo::OPERATION::UNIVERSAL;
				}
				else if (input.IsKeyPressed(KeyCode::KEY_Q))
				{
					viewport->GetGizmoOperation() = -1;
				}
			}

			if (viewport->GetGizmoOperation() == -1)
			{
				return;
			}

			if (camera->GetScene()->GetSelectedEntities().empty())
			{
				return;
			}

			const std::shared_ptr<Entity> entity = *camera->GetScene()->GetSelectedEntities().begin();
			if (!entity || !entity->IsValid())
			{
				camera->GetScene()->GetSelectedEntities().erase(entity);
				return;
			}

			ImGuizmo::SetOrthographic(true);
			ImGuizmo::SetDrawlist();

			ImGuizmo::SetRect(position.x, position.y, size.x, size.y);

			Camera& cameraComponent = camera->GetComponent<Camera>();
			glm::mat4 projectionMat4 = viewport->GetProjectionMat4();
			glm::mat4 viewMat4 = cameraComponent.GetViewMat4();
			Transform& transform = entity->GetComponent<Transform>();
			Transform& cameraTransform = camera->GetComponent<Transform>();

			if (glm::dot(cameraTransform.GetForward(), transform.GetPosition() - cameraTransform.GetPosition()) < 0)
			{
				return;
			}

			glm::mat4 transformMat4 = transform.GetTransform();
			ImGuizmo::Manipulate(glm::value_ptr(viewMat4), glm::value_ptr(projectionMat4),
				(ImGuizmo::OPERATION)viewport->GetGizmoOperation(), ImGuizmo::LOCAL, glm::value_ptr(transformMat4));

			active = ImGuizmo::IsUsing();
			if (active)
			{
				glm::vec3 position, rotation, scale;

				transformMat4 = glm::inverse(transform.GetTransform() *
					glm::inverse(transform.GetTransform(Transform::System::LOCAL))) *
					transformMat4;

				Utils::DecomposeTransform(transformMat4, position, rotation, scale);

				if (isSnapEnabled)
				{
					position = glm::round(position / snap) * snap;
				}

				transform.Translate(position);
				transform.Rotate(rotation);
				transform.Scale(scale);
			}
		};

		viewport->SetDrawGizmosCallback(callback);
	}
}

void Editor::MoveCamera(const std::shared_ptr<Entity>& camera, Window& window)
{
	if (!camera)
	{
		return;
	}

	Transform& transform = camera->GetComponent<Transform>();

	glm::vec3 rotation = transform.GetRotation();
	if (rotation.y > glm::two_pi<float>() || rotation.y < -glm::two_pi<float>())
	{
		rotation.y = 0.0f;
	}
	if (rotation.x > glm::two_pi<float>() || rotation.x < -glm::two_pi<float>())
	{
		rotation.x = 0.0f;
	}

	transform.Rotate(glm::vec3(rotation.x, rotation.y, 0.0f));

	constexpr double rotationSpeed = 90.0f;

	Input& input = Input::GetInstance(&window);

	const glm::vec2 delta = input.GetMousePositionDelta() * rotationSpeed * Time::GetDeltaTime();

	transform.Rotate(glm::vec3(rotation.x - glm::radians(delta.y),
		rotation.y - glm::radians(delta.x), 0));

	constexpr float defaultSpeed = 2.0f;
	float speed = defaultSpeed;

	if (input.IsKeyDown(KeyCode::KEY_LEFT_SHIFT))
	{
		speed *= 10.0f;
	}

	if (input.IsKeyDown(KeyCode::KEY_W))
	{
		transform.Translate(transform.GetPosition() + transform.GetForward() * (float)Time::GetDeltaTime() * speed);
	}
	else if (input.IsKeyDown(KeyCode::KEY_S))
	{
		transform.Translate(transform.GetPosition() + transform.GetForward() * -(float)Time::GetDeltaTime() * speed);
	}
	if (input.IsKeyDown(KeyCode::KEY_D))
	{
		transform.Translate(transform.GetPosition() + transform.GetRight() * (float)Time::GetDeltaTime() * speed);
	}
	else if (input.IsKeyDown(KeyCode::KEY_A))
	{
		transform.Translate(transform.GetPosition() + transform.GetRight() * -(float)Time::GetDeltaTime() * speed);
	}

	if (input.IsKeyDown(KeyCode::KEY_Q))
	{
		transform.Translate(transform.GetPosition() + transform.GetUp() * -(float)Time::GetDeltaTime() * speed);
	}
	else if (input.IsKeyDown(KeyCode::KEY_E))
	{
		transform.Translate(transform.GetPosition() + transform.GetUp() * (float)Time::GetDeltaTime() * speed);
	}
}

ThumbnailAtlas::TileInfo Editor::GetFileIcon(const std::filesystem::path& filepath, const std::string& format)
{
	ThumbnailAtlas::TileInfo finalFileIconInfo{};
	finalFileIconInfo.uvStart = { 0.0f, 1.0f };
	finalFileIconInfo.uvEnd = { 1.0f, 0.0f };

	const std::filesystem::path editorImagesPath = std::filesystem::path("Editor") / "Images";

	auto getThumbnail = [&](Thumbnails::Type type, const std::filesystem::path& defaultFilepath)
	{
		ThumbnailAtlas::TileInfo fileIconInfo = m_Thumbnails.GetOrGenerateThumbnail(filepath, nullptr, type);
		if (fileIconInfo.atlasId)
		{
			finalFileIconInfo = fileIconInfo;
		}
		else
		{
			finalFileIconInfo.atlasId = (ImTextureID)TextureManager::GetInstance().Load(defaultFilepath)->GetId();
		}
	};

	if (std::filesystem::is_directory(filepath))
	{
		finalFileIconInfo.atlasId = (ImTextureID)TextureManager::GetInstance().Load(editorImagesPath / "FolderIcon.png")->GetId();
	}
	else if (format == FileFormats::Meta())
	{
		finalFileIconInfo.atlasId = (ImTextureID)TextureManager::GetInstance().Load(editorImagesPath / "MetaIcon.png")->GetId();
	}
	else if (format == FileFormats::Mat())
	{
		getThumbnail(Thumbnails::Type::MAT, editorImagesPath / "MaterialIcon.png");
	}
	else if (format == FileFormats::Mesh())
	{
		getThumbnail(Thumbnails::Type::MESH, editorImagesPath / "MeshIcon.png");
	}
	else if (format == FileFormats::Scene())
	{
		ThumbnailAtlas::TileInfo fileIconInfo = m_Thumbnails.TryGetThumbnail(filepath);
		if (fileIconInfo.atlasId)
		{
			finalFileIconInfo = fileIconInfo;
		}
		else
		{
			finalFileIconInfo.atlasId = (ImTextureID)TextureManager::GetInstance().Load(editorImagesPath / "FileIcon.png")->GetId();
		}
	}
	else if (format == FileFormats::Prefab())
	{
		getThumbnail(Thumbnails::Type::PREFAB, editorImagesPath / "FileIcon.png");
	}
	else if (FileFormats::IsTexture(format))
	{
		getThumbnail(Thumbnails::Type::TEXTURE, editorImagesPath / "FileIcon.png");
	}

	if (!finalFileIconInfo.atlasId)
	{
		finalFileIconInfo.atlasId = (ImTextureID)TextureManager::GetInstance().Load(editorImagesPath / "FileIcon.png")->GetId();
	}
	return finalFileIconInfo;
}

bool Editor::RunProcess(const std::filesystem::path& executable, const std::filesystem::path& workingDir, const std::string& arguments)
{
	std::filesystem::path originalDir = std::filesystem::current_path();

	try
	{
		std::filesystem::current_path(workingDir);

		std::string command;

#ifdef _WIN32
		command = "\"" + executable.string() + "\"";
#endif

		if (!arguments.empty())
		{
			command += " " + arguments;
		}

		int result = std::system(command.c_str());

		std::filesystem::current_path(originalDir);

		return result == 0;

	}
	catch (const std::exception& e)
	{
		std::filesystem::current_path(originalDir);
		Logger::Error(e.what());
		return false;
	}
}

void Editor::SaveScene(std::shared_ptr<Scene> scene)
{
	std::string sceneFilepath = scene->GetFilepath().string();
	if (sceneFilepath == none)
	{
		sceneFilepath = "Scenes/" + scene->GetName() + FileFormats::Scene();
	}
	Serializer::SerializeScene(sceneFilepath, scene);
	m_Thumbnails.GetOrGenerateThumbnail(m_RootDirectory / sceneFilepath, scene, Thumbnails::Type::SCENE);
}

void Editor::PlayButtonMenu(std::shared_ptr<Scene> scene)
{
	const std::filesystem::path editorImagesPath = std::filesystem::path("Editor") / "Images";
	const auto playButtonTexture = TextureManager::GetInstance().Load(editorImagesPath / "PlayButton.png");
	const auto stopButtonTexture = TextureManager::GetInstance().Load(editorImagesPath / "StopButton.png");

	ImGui::Begin("Play Button Menu", nullptr, ImGuiWindowFlags_NoDecoration);

	if (m_RuntimeState == RuntimeState::STOP)
	{
		if (ImGui::ImageButton(playButtonTexture->GetId(), { 32, 32 }))
		{
			//m_RuntimeState = RuntimeState::PLAY;

			RunProcess("C:\\Repositories\\Pengine-2.0\\Build\\SandBox\\Build\\Release\\SandBox.exe", std::filesystem::current_path());
		}
	}
	//else if (m_RuntimeState == RuntimeState::PLAY)
	//{
	//	if (ImGui::ImageButton(stopButtonTexture->GetId(), { 32, 32 }))
	//	{
	//		m_RuntimeState = RuntimeState::STOP;
	//	}
	//}
	
	ImGui::End();
}

void Editor::DrawBitMask(const std::string& label, void* bitMask, size_t bitCount, size_t maxRows)
{
	ImGui::Text(label.c_str());

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));

	ImGui::PushID((label + "SetAll").c_str());
	if (ImGui::Button("", { 16, 16 }))
	{
		for (size_t i = 0; i < bitCount; i++)
		{
			*(size_t*)bitMask |= (1 << i);
		}
	}
	ImGui::PopID();

	ImGui::PopStyleColor(3);

	ImGui::SameLine();

	ImGui::PushID((label + "ClearAll").c_str());
	if (ImGui::Button("", { 16, 16 }))
	{
		for (size_t i = 0; i < bitCount; i++)
		{
			*(size_t*)bitMask &= ~(1 << i);
		}
	}
	ImGui::PopID();

	Indent indent;

	size_t columns = bitCount / maxRows;
	size_t rows = maxRows;

	if (columns == 0)
	{
		rows = 1;
		columns = bitCount;
	}

	for (size_t i = 0; i < rows; i++)
	{
		for (size_t j = 0; j < columns; j++)
		{
			size_t index = i * columns + j;

			bool bit = (*(size_t*)bitMask >> index) & 1;

			if (bit)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));
			}

			ImGui::PushID((label + std::to_string(index)).c_str());
			if (ImGui::Button("", { 16, 16 }))
			{
				*(size_t*)bitMask ^= (1 << index);
			}
			ImGui::PopID();

			if (bit)
			{
				ImGui::PopStyleColor(3);
			}

			ImGui::SameLine();
		}

		ImGui::NewLine();
	}
}

void Editor::ComponentsPopUpMenu(const std::shared_ptr<Entity>& entity)
{
	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::MenuItem("Camera"))
		{
			if (entity->HasComponent<Transform>())
			{
				entity->AddComponent<Camera>(entity);
			}
		}
		if (ImGui::MenuItem("Renderer3D"))
		{
			entity->AddComponent<Renderer3D>();
		}
		else if (ImGui::MenuItem("Decal"))
		{
			entity->AddComponent<Decal>();
		}
		else if (ImGui::MenuItem("PointLight"))
		{
			entity->AddComponent<PointLight>();
		}
		else if (ImGui::MenuItem("SpotLight"))
		{
			entity->AddComponent<SpotLight>();
		}
		else if (ImGui::MenuItem("DirectionalLight"))
		{
			entity->AddComponent<DirectionalLight>();
		}
		else if (ImGui::MenuItem("SkeletalAnimator"))
		{
			entity->AddComponent<SkeletalAnimator>();
		}
		else if (ImGui::MenuItem("EntityAnimator"))
		{
			entity->AddComponent<EntityAnimator>();
		}
		else if (ImGui::MenuItem("Canvas"))
		{
			entity->AddComponent<Canvas>();
		}
		else if (ImGui::MenuItem("RigidBody"))
		{
			entity->AddComponent<RigidBody>();
		}

		ReflectionSystem& reflectionSystem = ReflectionSystem::GetInstance();
		for (auto& [id, registeredClass] : reflectionSystem.m_ClassesByType)
		{
			if (ImGui::MenuItem(registeredClass.m_TypeInfo.name().data()))
			{
				registeredClass.m_CreateCallback(entity->GetRegistry(), entity->GetHandle());
			}
		}

		ImGui::EndPopup();
	}
}

void Editor::MainMenuBar(const std::shared_ptr<Scene>& scene)
{
	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
	ImGui::SetNextWindowClass(&windowClass);

	ImGui::Begin("MainMenuBar", nullptr,
		ImGuiWindowFlags_MenuBar
		| ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoMove);

	ImGui::BeginMenuBar();
	if (ImGui::BeginMenu("Create"))
	{
		if (ImGui::MenuItem("Viewport"))
		{
			m_CreateViewportMenu.opened = true;
			m_CreateViewportMenu.name[0] = '\0';
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Tools"))
	{
		if (ImGui::MenuItem("Save Materials"))
		{
			MaterialManager::GetInstance().SaveAll();
		}
		if (ImGui::MenuItem("Reload BaseMaterials"))
		{
			MaterialManager::GetInstance().Reload(MaterialManager::Category::BaseMaterial);
		}
		if (ImGui::MenuItem("Reload Materials"))
		{
			MaterialManager::GetInstance().Reload(MaterialManager::Category::Material);
		}
		if (ImGui::MenuItem("Reload UUIDs"))
		{
			auto& globalDataAccessor = GlobalDataAccessor::GetInstance();
			globalDataAccessor.GetFilepathByUuid().clear();
			globalDataAccessor.GetUuidByFilepath().clear();
			Serializer::GenerateFilesUUID(std::filesystem::current_path());
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Create"))
	{
		if (ImGui::MenuItem("Create Gameobject"))
		{
			const std::shared_ptr<Entity> entity = scene->CreateEntity();
			entity->AddComponent<Transform>(entity);
		}
		if (ImGui::MenuItem("Cube"))
		{
			scene->CreateCube();
		}
		if (ImGui::MenuItem("Sphere"))
		{
			scene->CreateSphere();
		}
		if (ImGui::MenuItem("Camera"))
		{
			scene->CreateCamera();
		}
		if (ImGui::MenuItem("Directional Light"))
		{
			scene->CreateDirectionalLight();
		}
		if (ImGui::MenuItem("Point Light"))
		{
			scene->CreatePointLight();
		}
		if (ImGui::MenuItem("Spot Light"))
		{
			scene->CreateSpotLight();
		}
		if (ImGui::MenuItem("Canvas"))
		{
			scene->CreateCanvas();
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Windows"))
	{
		if (ImGui::MenuItem("Entity Animator"))
		{
			m_EntityAnimatorEditorOpened = true;
		}
		ImGui::EndMenu();
	}
	ImGui::EndMenuBar();

	ImGui::End();
}

void Editor::Renderer3DComponent(const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<Renderer3D>())
	{
		return;
	}

	Renderer3D& r3d = entity->GetComponent<Renderer3D>();

	ImGui::PushID("Renderer3D X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<Renderer3D>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();
	
	ImGui::SameLine();
	
	if (ImGui::CollapsingHeader("Renderer3D"))
	{
		Indent indent;

		if (r3d.mesh && r3d.mesh->GetType() == Mesh::Type::SKINNED)
		{
			if (r3d.skeletalAnimatorEntityUUID.IsValid())
			{
				const auto skeletalAnimatorEntity = entity->GetScene()->FindEntityByUUID(r3d.skeletalAnimatorEntityUUID);
				if (skeletalAnimatorEntity)
				{
					ImGui::Text("Skeletal Animator: %s", skeletalAnimatorEntity->GetName().c_str());
				}
				else
				{
					ImGui::Text("Skeletal Animator: %s", none);
				}

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT"))
					{
						UUID* uuidPtr = (UUID*)payload->Data;
						r3d.skeletalAnimatorEntityUUID = *uuidPtr;
					}
					ImGui::EndDragDropTarget();
				}
			}
		}

		ImGui::Text("Mesh:");
		ImGui::SameLine();
		if (r3d.mesh)
		{
			if (ImGui::Button(r3d.mesh->GetName().c_str()))
			{
				m_MeshMenu.opened = true;
				m_MeshMenu.mesh = r3d.mesh;
			}
		}
		else
		{
			ImGui::Button(none);
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));

				if (FileFormats::Mesh() == Utils::GetFileFormat(filepath))
				{
					r3d.mesh = MeshManager::GetInstance().LoadMesh(Utils::GetShortFilepath(filepath));
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::Text("Material:");
		ImGui::SameLine();
		if (r3d.material)
		{
			if (ImGui::Button(r3d.material->GetName().c_str()))
			{
				m_MaterialMenu.opened = true;
				m_MaterialMenu.material = r3d.material;
			}
		}
		else
		{
			ImGui::Button(none);
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));

				if (Utils::GetFileFormat(filepath) == FileFormats::Mat())
				{
					r3d.material = MaterialManager::GetInstance().LoadMaterial(filepath);
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::PushID("R3D Is Enabled");
		ImGui::Checkbox("Is Enabled", &r3d.isEnabled);
		ImGui::PopID();

		ImGui::PushID("R3D Cast Shadows");
		ImGui::Checkbox("Cast Shadows", &r3d.castShadows);
		ImGui::PopID();

		const char* const renderingOrder[] = { "-5", "-4", "-3", "-2", "-1", "0", "1", "2", "3", "4", "5", };
		ImGui::PushID("R3D Rendering Order");
		ImGui::Combo("Rendering Order", &r3d.renderingOrder, renderingOrder, 11);
		ImGui::PopID();

		DrawBitMask("Object Visibility Mask", &r3d.objectVisibilityMask, 8, 2);
		DrawBitMask("Shadow Visibility Mask", &r3d.shadowVisibilityMask, 8, 2);
	}
}

void Editor::PointLightComponent(const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<PointLight>())
	{
		return;
	}

	PointLight& pointLight = entity->GetComponent<PointLight>();

	ImGui::PushID("PointLight X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<PointLight>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();

	ImGui::SameLine();

	if (ImGui::CollapsingHeader("PointLight"))
	{
		Indent indent;

		ImGui::ColorEdit3("Color", &pointLight.color[0]);
		ImGui::SliderFloat("Intensity", &pointLight.intensity, 0.0f, 10.0f);
		ImGui::SliderFloat("Radius", &pointLight.radius, 0.0f, 10.0f);
		ImGui::SliderFloat("Bias", &pointLight.bias, 0.001f, 0.1f);
		ImGui::Checkbox("Cast Shadows", &pointLight.castShadows);
		ImGui::Checkbox("Cast SSS", &pointLight.castSSS);
		ImGui::Checkbox("Draw Bounding Sphere", &pointLight.drawBoundingSphere);
	}
}

void Editor::SpotLightComponent(const std::shared_ptr<Pengine::Entity>& entity)
{
	if (!entity->HasComponent<SpotLight>())
	{
		return;
	}

	SpotLight& spotLight = entity->GetComponent<SpotLight>();

	ImGui::PushID("SpotLight X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<SpotLight>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();

	ImGui::SameLine();

	if (ImGui::CollapsingHeader("SpotLight"))
	{
		Indent indent;

		ImGui::ColorEdit3("Color", &spotLight.color[0]);
		ImGui::SliderFloat("Intensity", &spotLight.intensity, 0.0f, 10.0f);
		ImGui::SliderFloat("Radius", &spotLight.radius, 0.0f, 10.0f);
		ImGui::SliderFloat("Bias", &spotLight.bias, 0.001f, 0.1f);
		ImGui::SliderAngle("Inner CutOff", &spotLight.innerCutOff, 0.0f, 90.0f);
		ImGui::SliderAngle("Outer CutOff", &spotLight.outerCutOff, 0.0f, 90.0f);
		ImGui::Checkbox("Cast Shadows", &spotLight.castShadows);
		ImGui::Checkbox("Cast SSS", &spotLight.castSSS);
		ImGui::Checkbox("Draw Bounding Sphere", &spotLight.drawBoundingSphere);
	}
}

void Editor::DirectionalLightComponent(const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<DirectionalLight>())
	{
		return;
	}

	DirectionalLight& directionalLight = entity->GetComponent<DirectionalLight>();

	ImGui::PushID("DirectionalLight X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<DirectionalLight>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();
	
	ImGui::SameLine();
	
	if (ImGui::CollapsingHeader("DirectionalLight"))
	{
		Indent indent;

		ImGui::ColorEdit3("Color", &directionalLight.color[0]);
		ImGui::SliderFloat("Intensity", &directionalLight.intensity, 0.0f, 10.0f);
		ImGui::SliderFloat("Ambient", &directionalLight.ambient, 0.0f, 0.3f);
	}
}

void Editor::SkeletalAnimatorComponent(const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<SkeletalAnimator>())
	{
		return;
	}

	SkeletalAnimator& skeletalAnimator = entity->GetComponent<SkeletalAnimator>();

	ImGui::PushID("SkeletalAnimator X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<SkeletalAnimator>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();

	ImGui::SameLine();
	
	if (ImGui::CollapsingHeader("SkeletalAnimator"))
	{
		Indent indent;

		if (skeletalAnimator.GetSkeleton())
		{
			ImGui::Text("Skeleton: %s", skeletalAnimator.GetSkeleton()->GetName().c_str());
		}
		else
		{
			ImGui::Text("Skeleton: %s", none);
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));

				if (Utils::GetFileFormat(filepath) == FileFormats::Skeleton())
				{
					skeletalAnimator.SetSkeleton(MeshManager::GetInstance().LoadSkeleton(Utils::GetShortFilepath(filepath)));
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::Text("Animation:");
		ImGui::SameLine();
		if (skeletalAnimator.GetSkeletalAnimation())
		{
			ImGui::Button(skeletalAnimator.GetSkeletalAnimation()->GetName().c_str());
		}
		else
		{
			ImGui::Button(none);
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));

				if (Utils::GetFileFormat(filepath) == FileFormats::Anim())
				{
					skeletalAnimator.SetSkeletalAnimation(MeshManager::GetInstance().LoadSkeletalAnimation(filepath));
				}
			}

			ImGui::EndDragDropTarget();
		}

		float speed = skeletalAnimator.GetSpeed();
		if (ImGui::SliderFloat("Animation Speed", &speed, 0.0f, 5.0f))
		{
			skeletalAnimator.SetSpeed(speed);
		}

		if (skeletalAnimator.GetSkeletalAnimation())
		{
			float currentTime = skeletalAnimator.GetCurrentTime();
			if (ImGui::SliderFloat("Animation Current Time", &currentTime, 0.0f, skeletalAnimator.GetSkeletalAnimation()->GetDuration()))
			{
				skeletalAnimator.SetCurrentTime(currentTime);
			}
		}

		bool applySkeletonTransform = skeletalAnimator.GetApplySkeletonTransform();
		if (ImGui::Checkbox("Apply Skeleton Transform", &applySkeletonTransform))
		{
			skeletalAnimator.SetApplySkeletonTransform(applySkeletonTransform);
		}

		bool drawDebugSkeleton = skeletalAnimator.GetDrawDebugSkeleton();
		if (ImGui::Checkbox("Draw Debug Skeleton", &drawDebugSkeleton))
		{
			skeletalAnimator.SetDrawDebugSkeleton(drawDebugSkeleton);
		}
	}
}

void Editor::EntityAnimatorComponent(const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<EntityAnimator>())
	{
		return;
	}

	EntityAnimator& entityAnimator = entity->GetComponent<EntityAnimator>();

	ImGui::PushID("EntityAnimator X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<EntityAnimator>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();

	ImGui::SameLine();

	if (ImGui::CollapsingHeader("EntityAnimator"))
	{
		Indent indent;

		ImGui::Checkbox("Is Playing", &entityAnimator.isPlaying);
		ImGui::Checkbox("Is Loop", &entityAnimator.isLoop);
		ImGui::SliderFloat("Speed", &entityAnimator.speed, 0.0f, 10.0f);

		if (entityAnimator.animationTrack && !entityAnimator.animationTrack->keyframes.empty())
		{
			ImGui::SliderFloat("Time", &entityAnimator.time, 0.0f, entityAnimator.animationTrack->keyframes.back().time);
		}

		if (entityAnimator.animationTrack)
		{
			ImGui::Text("Animation Track: %s", entityAnimator.animationTrack->GetName().c_str());
		}
		else
		{
			ImGui::Text("Animation Track: %s", none);
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));

				if (Utils::GetFileFormat(filepath) == FileFormats::Track())
				{
					entityAnimator.animationTrack = Serializer::DeserializeAnimationTrack(filepath);
				}
			}

			ImGui::EndDragDropTarget();
		}
	}
}

void Editor::CanvasComponent(const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<Canvas>())
	{
		return;
	}

	Canvas& canvas = entity->GetComponent<Canvas>();

	ImGui::PushID("Canvas X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<Canvas>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();

	ImGui::SameLine();

	if (ImGui::CollapsingHeader("Canvas"))
	{
		Indent indent;

		ImGui::Checkbox("Draw In Main Viewport", &canvas.drawInMainViewport);
		DrawIVec2Control("Size", canvas.size, 0.0f, { 0, 1024 });

		if (ImGui::BeginMenu("Scripts"))
		{
			for (const auto& [name, callback] : ClayManager::GetInstance().scriptsByName)
			{
				if (ImGui::MenuItem(name.c_str()))
				{
					Canvas::Script& script = canvas.scripts.emplace_back();
					script.callback = callback;
					script.name = name;
				}
			}

			ImGui::EndMenu();
		}

		for (size_t i = 0; i < canvas.scripts.size(); i++)
		{
			std::string pushId = std::format("Remove Script UI {}", i);
			ImGui::PushID(pushId.c_str());
			if (ImGui::Button("X"))
			{
				canvas.scripts.erase(canvas.scripts.begin() + i);

				ImGui::PopID();
				break;
			}
			ImGui::PopID();

			ImGui::SameLine();

			ImGui::Text("%s", canvas.scripts[i].name.c_str());
		}
	}
}

void Editor::PhysicsBoxComponent(const std::shared_ptr<Entity>& entity)
{
	if (!entity->HasComponent<RigidBody>())
	{
		return;
	}

	RigidBody& rigidBody = entity->GetComponent<RigidBody>();

	ImGui::PushID("RigidBody X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<RigidBody>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();

	ImGui::SameLine();

	if (ImGui::CollapsingHeader("RigidBody"))
	{
		Indent indent;

		ImGui::Checkbox("Is Static", &rigidBody.isStatic);
		ImGui::Checkbox("Is Valid", &rigidBody.isValid);

		auto& physicsSystem = entity->GetScene()->GetPhysicsSystem()->GetInstance();

		int type = (int)rigidBody.type;
		const char* types[] = { "Box", "Sphere", "Cylinder" };
		ImGui::PushID("PhysicsBodyType");
		if (ImGui::Combo("Type", &type, types, 3))
		{
			rigidBody.type = (RigidBody::Type)type;
			switch (rigidBody.type)
			{
			case RigidBody::Type::Box:
			{
				rigidBody.shape.box = RigidBody::Box();
				rigidBody.isValid = false;
				break;
			}
			case RigidBody::Type::Sphere:
			{
				rigidBody.shape.sphere = RigidBody::Sphere();
				rigidBody.isValid = false;
				break;
			}
			case RigidBody::Type::Cylinder:
			{
				rigidBody.shape.cylinder = RigidBody::Cylinder();
				rigidBody.isValid = false;
				break;
			}
			}
		}
		ImGui::PopID();

		switch (rigidBody.type)
		{
		case RigidBody::Type::Box:
		{
			if (DrawVec3Control("Half Extents", rigidBody.shape.box.halfExtents, 1.0f, { 0.01f, 10.0f }))
			{
				rigidBody.shape.box.halfExtents = glm::max(rigidBody.shape.box.halfExtents, glm::vec3(0.01f));
				rigidBody.isValid = false;
			}
			break;
		}
		case RigidBody::Type::Sphere:
		{
			if (ImGui::SliderFloat("Radius", &rigidBody.shape.sphere.radius, 0.01f, 10.0f))
			{
				rigidBody.shape.sphere.radius = glm::max(rigidBody.shape.sphere.radius, 0.01f);
				rigidBody.isValid = false;
			}
			break;
		}
		case RigidBody::Type::Cylinder:
		{
			if (ImGui::SliderFloat("Radius", &rigidBody.shape.cylinder.radius, 0.01f, 10.0f))
			{
				rigidBody.shape.cylinder.radius = glm::max(rigidBody.shape.cylinder.radius, 0.01f);
				rigidBody.isValid = false;
			}
			if (ImGui::SliderFloat("Half Height", &rigidBody.shape.cylinder.halfHeight, 0.01f, 10.0f))
			{
				rigidBody.shape.cylinder.halfHeight = glm::max(rigidBody.shape.cylinder.halfHeight, 0.01f);
				rigidBody.isValid = false;
			}
			break;
		}
		}

		if (ImGui::SliderFloat("Mass", &rigidBody.mass, 0.0f, 10.0f))
		{
			rigidBody.isValid = false;
		}

		JPH::BodyLockWrite lock(physicsSystem.GetBodyLockInterface(), rigidBody.id);
		if (lock.Succeeded())
		{
			JPH::Body& body = lock.GetBody();

			float friction = body.GetFriction();
			if (ImGui::SliderFloat("Friction", &friction, 0.0f, 1.0f));
			{
				body.SetFriction(friction);
			}

			float restitution = body.GetRestitution();
			if (ImGui::SliderFloat("Restitution", &restitution, 0.0f, 1.0f));
			{
				body.SetRestitution(restitution);
			}

			glm::vec3 angularVelocity = JoltVec3ToGlmVec3(body.GetAngularVelocity());
			if (DrawVec3Control("Angular Velocity", angularVelocity));
			{
				body.SetAngularVelocity(GlmVec3ToJoltVec3(angularVelocity));
			}

			glm::vec3 linearVelocity = JoltVec3ToGlmVec3(body.GetLinearVelocity());
			if (DrawVec3Control("Linear Velocity", linearVelocity));
			{
				body.SetLinearVelocity(GlmVec3ToJoltVec3(linearVelocity));
			}

			bool allowSleeping = body.GetAllowSleeping();
			if (ImGui::Checkbox("Allow Sleeping", &allowSleeping))
			{
				body.SetAllowSleeping(allowSleeping);
			}
		}
		lock.ReleaseLock();
	}
}

void Editor::DecalComponent(const std::shared_ptr<Pengine::Entity>& entity)
{
	if (!entity->HasComponent<Decal>())
	{
		return;
	}

	Decal& decal = entity->GetComponent<Decal>();

	ImGui::PushID("Decal X");
	if (ImGui::Button("X"))
	{
		std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>([entity]()
		{
			entity->RemoveComponent<Decal>();
		}, Event::Type::OnNextFrame, this);
		EventSystem::GetInstance().SendEvent(event);
	}
	ImGui::PopID();

	ImGui::SameLine();

	if (ImGui::CollapsingHeader("Decal"))
	{
		Indent indent;

		ImGui::Text("Material:");
		ImGui::SameLine();
		if (decal.material)
		{
			if (ImGui::Button(decal.material->GetName().c_str()))
			{
				m_MaterialMenu.opened = true;
				m_MaterialMenu.material = decal.material;
			}
		}
		else
		{
			ImGui::Button(none);
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));

				if (Utils::GetFileFormat(filepath) == FileFormats::Mat())
				{
					decal.material = MaterialManager::GetInstance().LoadMaterial(filepath);
				}
			}

			ImGui::EndDragDropTarget();
		}

		DrawBitMask("Object Visibility Mask", &decal.objectVisibilityMask, 8, 2);
	}
}

void Editor::UserComponents(const std::shared_ptr<Entity>& entity)
{
	ReflectionSystem& reflectionSystem = ReflectionSystem::GetInstance();

	std::function<void(void*, ReflectionSystem::RegisteredClass&, size_t)> properties = [this, &properties]
		(void* instance, ReflectionSystem::RegisteredClass& registeredClass, size_t offset)
	{
		for (auto& [name, prop] : registeredClass.m_PropertiesByName)
		{
#define SERIALIZE_PROPERTY(_type) \
if (prop.IsValue<_type>()) \
{ \
	const _type& value = prop.GetValue<_type>(instance, offset); \
	out << YAML::Key << "Type" << prop.m_Type; \
	out << YAML::Key << "Value" << value; \
}

			if (prop.IsValue<bool>())
			{
				ImGui::Checkbox(name.c_str(), &prop.GetValue<bool>(instance, offset));
			}
			else if(prop.IsValue<int>())
			{
				ImGui::SliderInt(name.c_str(), &prop.GetValue<int>(instance, offset), 0, 10);
			}
			else if (prop.IsValue<float>())
			{
				ImGui::SliderFloat(name.c_str(), &prop.GetValue<float>(instance, offset), 0.0f, 10.0f);
			}
			else if (prop.IsValue<double>())
			{
				ImGui::InputDouble(name.c_str(), &prop.GetValue<double>(instance, offset));
			}
			else if (prop.IsValue<std::string>())
			{
				char textBuffer[256];
				strcpy(textBuffer, prop.GetValue<std::string>(instance, offset).data());
				if (ImGui::InputText(name.c_str(), textBuffer, sizeof(textBuffer)))
				{
					prop.GetValue<std::string>(instance, offset) = textBuffer;
				}
			}
			else if (prop.IsValue<glm::vec2>())
			{
				DrawVec2Control(name, prop.GetValue<glm::vec2>(instance, offset));
			}
			else if (prop.IsValue<glm::vec3>())
			{
				DrawVec3Control(name, prop.GetValue<glm::vec3>(instance, offset));
			}
			else if (prop.IsValue<glm::vec4>())
			{
				DrawVec4Control(name, prop.GetValue<glm::vec4>(instance, offset));
			}
			else if (prop.IsValue<glm::ivec2>())
			{
				DrawIVec2Control(name, prop.GetValue<glm::ivec2>(instance, offset));
			}
			else if (prop.IsValue<glm::ivec3>())
			{
				DrawIVec3Control(name, prop.GetValue<glm::ivec3>(instance, offset));
			}
			else if (prop.IsValue<glm::ivec4>())
			{
				DrawIVec4Control(name, prop.GetValue<glm::ivec4>(instance, offset));
			}
		}

		for (const auto& parent : registeredClass.m_Parents)
		{
			ReflectionSystem& reflectionSystem = ReflectionSystem::GetInstance();
			auto classByType = reflectionSystem.m_ClassesByType.find(parent.first);
			if (classByType != reflectionSystem.m_ClassesByType.end())
			{
				properties(instance, classByType->second, parent.second);
			}
		}
	};

	for (auto [id, storage] : entity->GetRegistry().storage())
	{
		if (storage.contains(entity->GetHandle()))
		{
			auto classByType = reflectionSystem.m_ClassesByType.find(id);
			if (classByType == reflectionSystem.m_ClassesByType.end())
			{
				continue;
			}

			auto& registeredClass = classByType->second;
			void* component = storage.value(entity->GetHandle());

			ImGui::PushID(registeredClass.m_TypeInfo.name().data());
			if (ImGui::Button("X"))
			{
				if (registeredClass.m_RemoveCallback)
				{
					registeredClass.m_RemoveCallback(entity->GetRegistry(), entity->GetHandle());
					entity->NotifySceneAboutComponentRemove(registeredClass.m_TypeInfo.name().data());
				}
			}
			ImGui::PopID();

			ImGui::SameLine();

			if (ImGui::CollapsingHeader(registeredClass.m_TypeInfo.name().data()))
			{
				Indent indent;

				properties(component, registeredClass, 0);
			}
		}
	}
}

Editor::Indent::Indent()
{
	ImGui::Indent();
}

Editor::Indent::~Indent()
{
	ImGui::Unindent();
}

void Editor::MaterialMenu::Update(Editor& editor)
{
	if (!material)
	{
		return;
	}

	const bool previousOpened = opened;

	auto drawTexture = [this, &editor](
		const std::shared_ptr<Texture> texture,
		const ShaderReflection::ReflectDescriptorSetBinding& binding,
		const std::string& passName,
		bool& isChangedToSerialize)
	{
		ImGui::Text("%s", binding.name.c_str());
		ImGui::SameLine();
		ImGui::Text("%s", texture->GetFilepath().string().c_str());
		ImGui::Image(ImTextureID(texture->GetId()), { 128, 128 });

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));

				if (FileFormats::IsTexture(Utils::GetFileFormat(filepath)))
				{
					material->GetUniformWriter(passName)->WriteTextureToAllFrames(binding.name, TextureManager::GetInstance().Load(filepath));
					
					isChangedToSerialize = true;
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();

		const std::string whiteId = "Set White" + binding.name;
		ImGui::PushID(whiteId.c_str());
		if (ImGui::Button("White"))
		{
			material->GetUniformWriter(passName)->WriteTextureToAllFrames(binding.name, TextureManager::GetInstance().GetWhite());

			isChangedToSerialize = true;
		}
		ImGui::PopID();

		ImGui::SameLine();

		const std::string blackId = "Set Black" + binding.name;
		ImGui::PushID(blackId.c_str());
		if (ImGui::Button("Black"))
		{
			material->GetUniformWriter(passName)->WriteTextureToAllFrames(binding.name, TextureManager::GetInstance().GetBlack());

			isChangedToSerialize = true;
		}
		ImGui::PopID();
	};

	auto drawTextureVariable = [this, &editor](
		const std::shared_ptr<Texture> texture,
		const std::string& name,
		std::shared_ptr<Texture>& newTexture)
	{
		ImGui::Text("%s", name.c_str());
		ImGui::SameLine();
		ImGui::Text("%s", texture->GetFilepath().string().c_str());
		ImGui::Image(ImTextureID(texture->GetId()), { 128, 128 });

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSETS_BROWSER_ITEM"))
			{
				std::wstring filepath((const wchar_t*)payload->Data);
				filepath.resize(payload->DataSize / sizeof(wchar_t));

				if (FileFormats::IsTexture(Utils::GetFileFormat(filepath)))
				{
					newTexture = TextureManager::GetInstance().Load(filepath);
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();

		const std::string whiteId = "Set White" + name;
		ImGui::PushID(whiteId.c_str());
		if (ImGui::Button("White"))
		{
			newTexture = TextureManager::GetInstance().GetWhite();
		}
		ImGui::PopID();

		ImGui::SameLine();

		const std::string blackId = "Set Black" + name;
		ImGui::PushID(blackId.c_str());
		if (ImGui::Button("Black"))
		{
			newTexture = TextureManager::GetInstance().GetBlack();
		}
		ImGui::PopID();
	};

	if (opened && ImGui::Begin("Material", &opened))
	{
		if (ImGui::Button("Reload"))
		{
			Material::Reload(material);
		}

		ImGui::Text("Name: %s", material->GetName().c_str());
		ImGui::Text("Filepath: %s", material->GetFilepath().string().c_str());
		
		ImGui::Text("BaseMaterial: ");
		ImGui::SameLine();
		if (ImGui::Button(material->GetBaseMaterial()->GetFilepath().string().c_str()))
		{
			editor.m_BaseMaterialMenu.opened = true;
			editor.m_BaseMaterialMenu.baseMaterial = material->GetBaseMaterial();
		}

		bool isChangedToSerialize = false;

		auto optionsByName = material->GetOptionsByName();
		if (!optionsByName.empty() && ImGui::CollapsingHeader("Options"))
		{
			for (auto& [name, option] : optionsByName)
			{
				if (ImGui::Checkbox(name.c_str(), &option.m_IsEnabled))
				{
					material->SetOption(name, option.m_IsEnabled);

					isChangedToSerialize = true;
				}
			}
		}

		std::function<void(
			const ShaderReflection::ReflectVariable&, bool&, void*, Editor&, const std::shared_ptr<Material>&)> drawVariable = [
			&drawTextureVariable,
			&drawVariable
		](
			const ShaderReflection::ReflectVariable& variable,
			bool& isChanged,
			void* data,
			Editor& editor,
			const std::shared_ptr<Material>& material)
		{
			// Variables with names that contain any "color" part and types vec3 or vec4 will be considered as a color variable
			// and will be represented in the editor as color pickers.
			if (Utils::Contains(Utils::ToLower(variable.name), "color"))
			{
				if (variable.type == ShaderReflection::ReflectVariable::Type::VEC3)
				{
					isChanged += ImGui::ColorEdit3(variable.name.c_str(), &Utils::GetValue<glm::vec3>(data, variable.offset)[0]);
				}
				if (variable.type == ShaderReflection::ReflectVariable::Type::VEC4)
				{
					isChanged += ImGui::ColorEdit4(variable.name.c_str(), &Utils::GetValue<glm::vec4>(data, variable.offset)[0]);
				}

				return;
			}

			// Variable with names that contain any "use" part and types int will be considered as a bool variable
			// and will be represented in the editor as check boxes.
			if (Utils::Contains(Utils::ToLower(variable.name), "use"))
			{
				if (variable.type == ShaderReflection::ReflectVariable::Type::INT)
				{
					bool used = (bool)Utils::GetValue<int>(data, variable.offset);
					isChanged += ImGui::Checkbox(variable.name.c_str(), &used);
					Utils::GetValue<int>(data, variable.offset) = (int)used;
				}
				return;
			}

			if (variable.type == ShaderReflection::ReflectVariable::Type::FLOAT)
			{
				isChanged += ImGui::SliderFloat(variable.name.c_str(), &Utils::GetValue<float>(data, variable.offset), 0.0f, 1.0f);
			}
			if (variable.type == ShaderReflection::ReflectVariable::Type::INT)
			{
				isChanged += ImGui::InputInt(variable.name.c_str(), &Utils::GetValue<int>(data, variable.offset));
			}
			if (variable.type == ShaderReflection::ReflectVariable::Type::VEC2)
			{
				isChanged += editor.DrawVec2Control(variable.name.c_str(), Utils::GetValue<glm::vec2>(data, variable.offset));
			}
			if (variable.type == ShaderReflection::ReflectVariable::Type::VEC3)
			{
				isChanged += editor.DrawVec3Control(variable.name.c_str(), Utils::GetValue<glm::vec3>(data, variable.offset));
			}
			if (variable.type == ShaderReflection::ReflectVariable::Type::VEC4)
			{
				isChanged += editor.DrawVec4Control(variable.name.c_str(), Utils::GetValue<glm::vec4>(data, variable.offset));
			}
			if (variable.type == ShaderReflection::ReflectVariable::Type::TEXTURE)
			{
				int& bindlessTextureIndex = Utils::GetValue<int>(data, variable.offset);
				const auto texture = material->GetBindlessTexture(bindlessTextureIndex);

				std::shared_ptr<Texture> newTexture = texture;
				drawTextureVariable(texture, variable.name, newTexture);

				if (newTexture && newTexture != texture)
				{
					// Note: Lets assume that we never unbind manually, it will unbind on delete.
					//material->UnBindBindlessTexture(texture);
					bindlessTextureIndex = material->BindBindlessTexture(newTexture);
					isChanged += 1;
				}
			}
			if (variable.type == ShaderReflection::ReflectVariable::Type::STRUCT)
			{
				for (const auto& memberVariable : variable.variables)
				{
					drawVariable(memberVariable, isChanged, data, editor, material);
				}
			}
		};

		for (const auto& [passName, pipeline] : material->GetBaseMaterial()->GetPipelinesByPass())
		{
			if (ImGui::CollapsingHeader(passName.c_str()))
			{
				std::vector<std::shared_ptr<UniformLayout>> uniformLayouts;

				{
					std::optional<uint32_t> descriptorSetIndex = pipeline->GetDescriptorSetIndexByType(Pipeline::DescriptorSetIndexType::MATERIAL, passName);
					if (descriptorSetIndex)
					{
						uniformLayouts.emplace_back(pipeline->GetUniformLayout(*descriptorSetIndex));
					}
				}

				{
					std::optional<uint32_t> descriptorSetIndex = pipeline->GetDescriptorSetIndexByType(Pipeline::DescriptorSetIndexType::BUFFER_DEVICE_ADDRESS, passName);
					if (descriptorSetIndex)
					{
						uniformLayouts.emplace_back(pipeline->GetUniformLayout(*descriptorSetIndex));
					}
				}
				
				const std::shared_ptr<UniformWriter> uniformWriter = material->GetUniformWriter(passName);

				for (const auto& uniformLayout : uniformLayouts)
				{
					for (const auto& binding : uniformLayout->GetBindings())
					{
						if (uniformWriter && binding.type == ShaderReflection::Type::COMBINED_IMAGE_SAMPLER)
						{
							if (std::shared_ptr<Texture> texture = uniformWriter->GetTextureInfo(binding.name).back().texture)
							{
								drawTexture(texture, binding, passName, isChangedToSerialize);
							}
						}
						else if (binding.type == ShaderReflection::Type::UNIFORM_BUFFER || binding.type == ShaderReflection::Type::STORAGE_BUFFER)
						{
							const std::shared_ptr<Buffer> buffer = material->GetBuffer(binding.name);
							if (!buffer)
							{
								continue;
							}

							ImGui::Text("%s", binding.name.c_str());

							void* data = (char*)buffer->GetData();

							bool isChanged = false;
							for (const auto& variable : binding.buffer->variables)
							{
								drawVariable(variable, isChanged, data, editor, material);
							}

							if (isChanged)
							{
								// Need to mark for flush that the buffer was changed.
								buffer->WriteToBuffer(data, 0, 0);

								isChangedToSerialize = true;
							}
						}
					}
				}
			}
		}

		if (isChangedToSerialize)
		{
			Material::Save(material, false);
		}

		ImGui::End();
	}

	if (previousOpened && !opened)
	{
		MaterialManager::GetInstance().DeleteMaterial(material);
	}
}

void Editor::CreateFileMenu::Update()
{
	if (!opened)
	{
		return;
	}

	ImGui::SetNextWindowSize({ 360.0f, 60.0f });
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(), 0, { 0.5f, 0.0f });
	if (ImGui::Begin("Create File", &opened,
		ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::InputText("Filename", name, 64);
		
		ImGui::SameLine();

		if (ImGui::Button("Create"))
		{
			if (name[0] != '\0')
			{
				MakeFile(filepath / (name + format));

				opened = false;
			}
		}

		ImGui::End();
	}
}

void Editor::CreateFileMenu::MakeFile(const std::filesystem::path& filepath)
{
	std::ofstream out(filepath, std::ostream::binary);
	out.close();
}

void Editor::DeleteFileMenu::Update()
{
	if (!opened)
	{
		return;
	}

	ImGui::SetNextWindowSize({ 250.0f, 60.0f });
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(), 0, { 0.5f, 0.0f });
	if (ImGui::Begin("Deleting directory or file", &opened,
		ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text("Are you sure you want to delete\n%s?", filepath.filename().string().c_str());
		if (ImGui::Button("Yes"))
		{
			if (Utils::GetFileFormat(filepath.string()).empty())
			{
				std::filesystem::remove_all(filepath);
			}
			else
			{
				std::filesystem::remove(filepath);

				const std::filesystem::path metaFilepath = filepath.concat(FileFormats::Meta());
				if (std::filesystem::exists(metaFilepath))
				{
					std::filesystem::remove(metaFilepath);
				}
			}

			// TODO: Do it more optimal, fine for now, very slow.
			auto& globalDataAccessor = GlobalDataAccessor::GetInstance();
			globalDataAccessor.GetFilepathByUuid().clear();
			globalDataAccessor.GetUuidByFilepath().clear();
			Serializer::GenerateFilesUUID(std::filesystem::current_path());

			opened = false;
		}

		ImGui::SameLine();

		if (ImGui::Button("No"))
		{
			opened = false;
		}
		ImGui::End();
	}
}

void Editor::CreateViewportMenu::Update(const Editor& editor, Window& window)
{
	if (!opened)
	{
		return;
	}

	ImGui::SetNextWindowSize({ 350.0f, 80.0f });
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(), 0, { 0.5f, 0.0f });
	if (ImGui::Begin("Create Viewport", &opened,
		ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::InputText("Name", name, 64);
		
		ImGui::SameLine();

		if (ImGui::Button("Create"))
		{
			if (name[0] != '\0')
			{
				auto callback = [this, &window]()
				{
					if (const std::shared_ptr<Viewport> viewport = window.GetViewportManager().GetViewport(name))
					{
						return;
					}

					window.GetViewportManager().Create(name, size);
					opened = false;
					size = { 1024, 1024 };
					name[0] = '\0';
				};

				std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
				EventSystem::GetInstance().SendEvent(event);
			}
		}

		editor.DrawIVec2Control("Size", size, 1024, { 0, 2560 });

		ImGui::End();
	}
}

void Editor::BaseMaterialMenu::Update(const Editor& editor)
{
	if (!baseMaterial)
	{
		return;
	}

	if (opened && ImGui::Begin("BaseMaterial", &opened))
	{
		if (ImGui::Button("Reload"))
		{
			BaseMaterial::Reload(baseMaterial);
		}

		ImGui::Text("Name: %s", baseMaterial->GetName().c_str());
		ImGui::Text("Filepath: %s", baseMaterial->GetFilepath().string().c_str());

		ImGui::End();
	}
}

void Editor::MeshMenu::Update(const Editor& editor)
{
	if (!mesh)
	{
		return;
	}

	const bool previousOpened = opened;

	if (opened && ImGui::Begin("Mesh", &opened))
	{
		ImGui::Text("Name: %s", mesh->GetName().c_str());
		ImGui::Text("Filepath: %s", mesh->GetFilepath().string().c_str());

		const char* const types[] = { "Static", "Skinned" };
		ImGui::Text("Type %s", types[(int)mesh->GetType()]);

		BoundingBox aabb = mesh->GetBoundingBox();
		editor.DrawVec3Control("AABB min", aabb.min);
		editor.DrawVec3Control("AABB max", aabb.max);
		editor.DrawVec3Control("AABB offset", aabb.offset);
		mesh->SetBoundingBox(aabb);

		ImGui::Text("Vertex Count %u", mesh->GetVertexCount());

		const auto& lods = mesh->GetCreateInfo().lods;
		for (size_t i = 0; i < lods.size(); i++)
		{
			if (ImGui::CollapsingHeader(std::format("Lod {}", i).c_str()))
			{
				Indent indent;
				ImGui::Text("Index Count %u", lods[i].indexCount);
				ImGui::Text("Index Offset %u", lods[i].indexOffset);
				ImGui::Text("Triangle Count %u", lods[i].indexCount / 3);
			}
		}

		if (ImGui::Button("Save"))
		{
			Serializer::SerializeMesh(mesh->GetFilepath().parent_path(), mesh);
		}

		ImGui::End();
	}

	if (previousOpened && !opened)
	{
		MeshManager::GetInstance().DeleteMesh(mesh);
	}
}

void Editor::CloneMaterialMenu::Update()
{
	if (!opened)
	{
		return;
	}

	ImGui::SetNextWindowSize({ 450.0f, 70.0f });
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(), 0, { 0.5f, 0.0f });
	if (ImGui::Begin("Clone Material", &opened,
		ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text(std::string("Cloned Material: " + material->GetFilepath().string()).c_str());
		ImGui::InputText("Filename", name, 64);

		ImGui::SameLine();

		if (ImGui::Button("Clone"))
		{
			if (name[0] != '\0')
			{
				std::filesystem::path filepath = material->GetFilepath().parent_path() / name;
				filepath.replace_extension(FileFormats::Mat());
				std::shared_ptr<Material> clonedMaterial = MaterialManager::GetInstance().Clone(name, filepath, material);
				Material::Save(clonedMaterial);

				opened = false;
			}
		}

		ImGui::End();
	}
}

void Editor::LoadIntermediateMenu::Update()
{
	if (!opened)
	{
		return;
	}

	ImGui::SetNextWindowSize({ 450.0f, 70.0f });
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(), 0, { 0.5f, 0.0f });
	if (ImGui::Begin("Load Intermediate", nullptr,
		ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text(std::string("Work Name: " + workName).c_str());
		ImGui::ProgressBar(workStatus, ImVec2(ImGui::GetFontSize() * 25, 0.0f));
		ImGui::End();
	}
}

void Editor::TextureMetaPropertiesMenu::Update()
{
	if (!opened)
	{
		return;
	}

	if (opened && ImGui::Begin("Texture Meta Properties", &opened))
	{
		bool isChangedToSerialize = false;
		ImGui::Text("Filepath: %s", meta.filepath.string().c_str());
		isChangedToSerialize += ImGui::Checkbox("Create Mip Maps", &meta.createMipMaps);
		isChangedToSerialize += ImGui::Checkbox("SRGB", &meta.srgb);
		
		if (isChangedToSerialize)
		{
			Serializer::SerializeTextureMeta(meta);
		}

		ImGui::End();
	}
}

void Editor::ImportMenu::Update(Editor& editor)
{
	if (!opened)
	{
		return;
	}

	if (opened && ImGui::Begin("Import Settings", &opened))
	{
		ImGui::Text("Filepath: %s", importInfo.filepath.string().c_str());

		ImGui::Checkbox("Create Folder", &importOptions.createFolder);

		if (!importInfo.prefabs.empty() && ImGui::CollapsingHeader("Prefabs##Import"))
		{
			Indent indent;

			ImGui::Checkbox("Import##ImportPrefabs", &importOptions.prefabs);

			{
				Indent indent;
				for (const auto& prefab : importInfo.prefabs)
				{
					ImGui::Text(prefab.c_str());
				}
			}
		}

		if (!importInfo.meshes.empty() && ImGui::CollapsingHeader("Meshes##Import"))
		{
			Indent indent;

			ImGui::Checkbox("Import##ImportMeshes", &importOptions.meshes.import);
			ImGui::Checkbox("Skinned##ImportMeshes", &importOptions.meshes.skinned);
			ImGui::SliderFloat("Target Error##ImportMeshes", &importOptions.meshes.targetError, 0.0f, 0.1f);
			ImGui::SliderFloat("Min Index Count Factor##ImportMeshes", &importOptions.meshes.minIndexCountFactor, 0.05f, 1.0f);
			ImGui::SliderInt("Lod Count##ImportMeshes", &importOptions.meshes.lodCount, 1, 10);
			editor.DrawVec2Control("Min Max Switch Distance", importOptions.meshes.distanceMinMax, 0.0f, { 0.0f, 100.0f });
			ImGui::Checkbox("Flip UV X##ImportMeshes", &importOptions.meshes.flipUV.x);
			ImGui::SameLine();
			ImGui::Checkbox("Flip UV Y##ImportMeshes", &importOptions.meshes.flipUV.y);

			{
				Indent indent;
				for (const auto& mesh : importInfo.meshes)
				{
					ImGui::Text(mesh.c_str());
				}
			}
		}

		if (!importInfo.materials.empty() && ImGui::CollapsingHeader("Materials##Import"))
		{
			Indent indent;

			ImGui::Checkbox("Import##ImportMaterials", &importOptions.materials);

			{
				Indent indent;
				for (const auto& material : importInfo.materials)
				{
					ImGui::Text(material.c_str());
				}
			}
		}

		if (!importInfo.animations.empty() && ImGui::CollapsingHeader("Animations##Import"))
		{
			Indent indent;

			ImGui::Checkbox("Import##ImportAnimations", &importOptions.animations);

			{
				Indent indent;
				for (const auto& animation : importInfo.animations)
				{
					ImGui::Text(animation.c_str());
				}
			}
		}

		if (!importInfo.skeletons.empty() && ImGui::CollapsingHeader("Skeletons##Import"))
		{
			Indent indent;

			ImGui::Checkbox("Import##ImportSkeletons", &importOptions.skeletons);

			{
				Indent indent;
				for (const auto& skeleton : importInfo.skeletons)
				{
					ImGui::Text(skeleton.c_str());
				}
			}
		}

		if (ImGui::Button("Import##Button"))
		{
			opened = false;

			editor.m_LoadIntermediateMenu.opened = true;
			editor.m_LoadIntermediateMenu.workStatus = 0;
			editor.m_LoadIntermediateMenu.workName.clear();

			ThreadPool::GetInstance().EnqueueAsync([&editor, this]()
			{
				importOptions.filepath = Utils::Erase(importInfo.filepath.string(), editor.m_RootDirectory.string() + "/");
				Serializer::LoadIntermediate(
					importOptions,
					editor.m_LoadIntermediateMenu.workName,
					editor.m_LoadIntermediateMenu.workStatus);

				editor.m_LoadIntermediateMenu.opened = false;

				importInfo = {};
			});
		}

		ImGui::End();
	}
}

void Editor::Thumbnails::Initialize()
{
	const std::string name = "Thumbnail";

	m_ThumbnailRenderer = Renderer::Create();
	m_ThumbnailScene = SceneManager::GetInstance().Create(name, name);
	m_ThumbnailWindow = Window::CreateHeadless(name, name, { 256, 256 });

	m_ThumbnailScene->SetGraphicsSettings(Serializer::DeserializeGraphicsSettings(std::filesystem::path("Configs") / "GraphicsSettingsHigh.gs"));
	m_ThumbnailScene->GetSettings().drawBoundingBoxes = false;

	{
		auto entity = m_ThumbnailScene->CreateEntity("Sun");
		auto& transform = entity->AddComponent<Transform>(entity);
		transform.Rotate(glm::radians(glm::vec3(120.0f, -40.0f, 0.0f)));

		entity->AddComponent<DirectionalLight>().intensity = 5.0f;
	}

	auto camera = m_ThumbnailScene->CreateEntity("Camera");
	auto& cameraComponent =	camera->AddComponent<Camera>(camera);
	{
		m_CameraUUID = camera->GetUUID();
		cameraComponent.CreateRenderView(name, m_ThumbnailWindow->GetSize());
		camera->AddComponent<Transform>(camera);
	}

	{
		auto entity = m_ThumbnailScene->CreateEntity("Entity");
		entity->AddComponent<Transform>(entity);
		auto& r3d = entity->AddComponent<Renderer3D>();
		r3d.mesh = MeshManager::GetInstance().LoadMesh(std::filesystem::path("Meshes") / "Sphere.mesh");
		r3d.material = MaterialManager::GetInstance().LoadMaterial(std::filesystem::path("Materials") / "MeshBaseDoubleSided.mat");
	}

	auto& globalDataAccessor = GlobalDataAccessor::GetInstance();
	uint32_t& frameInFlightCount = globalDataAccessor.GetFrameInFlightCount();
	uint32_t& frameInFlightIndex = globalDataAccessor.GetFrameInFlightIndex();

	const uint32_t previousFrameInFlightIndex = frameInFlightIndex;

	// Need to render n times to initialize every render target and etc.
	for (size_t i = 0; i < frameInFlightCount; i++)
	{
		// SetCamera and other functions in Renderer::Update send callbacks to create render target
		// and other resources on the next frame,
		// but we need it now, so we explicitly process events now.
		EventSystem::GetInstance().ProcessEvents();

		std::map<std::shared_ptr<Scene>, std::vector<Renderer::RenderViewportInfo>> viewportsByScene;
		const std::shared_ptr<Scene> scene = camera->GetScene();

		Renderer::RenderViewportInfo renderViewportInfo{};
		renderViewportInfo.camera = camera;
		renderViewportInfo.renderView = cameraComponent.GetRendererTarget(name);
		renderViewportInfo.size = m_ThumbnailWindow->GetSize();

		const float aspect = (float)renderViewportInfo.size.x / (float)renderViewportInfo.size.y;
		renderViewportInfo.projection = glm::perspectiveRH_ZO(cameraComponent.GetFov(), aspect, cameraComponent.GetZFar(), cameraComponent.GetZNear());

		viewportsByScene[scene].emplace_back(renderViewportInfo);

		void* frame = m_ThumbnailWindow->BeginFrame();

		m_ThumbnailRenderer->Update(frame, m_ThumbnailWindow, m_ThumbnailRenderer, viewportsByScene);

		m_ThumbnailWindow->EndFrame(frame);

		frameInFlightIndex = ++frameInFlightIndex % frameInFlightCount;
	}

	Pengine::GlobalDataAccessor::GetInstance().GetDevice()->WaitIdle();

	frameInFlightIndex = previousFrameInFlightIndex;
}

void Editor::Thumbnails::ShutDown()
{
	m_ThumbnailWindow = nullptr;
	SceneManager::GetInstance().Delete(m_ThumbnailScene);
	m_ThumbnailRenderer = nullptr;
}

void Editor::Thumbnails::UpdateThumbnails()
{
	/*if (m_ThumbnailAtlas.GetAtlas(0))
	{
		ImGui::Begin("Thumbnail Atlas");
		ImGui::Image((ImTextureID)m_ThumbnailAtlas.GetAtlas(0)->GetId(), ImVec2(1024, 1024));
		ImGui::End();
	}*/

	if (m_ThumbnailToCheck == m_CacheThumbnails.end())
	{
		m_ThumbnailToCheck = m_CacheThumbnails.begin();
	}
	else
	{
		m_ThumbnailToCheck++;
	}

	if (m_ThumbnailQueue.empty())
	{
		return;
	}

	if (!std::filesystem::exists("Thumbnails"))
	{
		std::filesystem::create_directory("Thumbnails");
	}

	const ThumbnailLoadInfo thumbnailLoadInfo = m_ThumbnailQueue.front();
	m_ThumbnailQueue.pop_front();

	m_ThumbnailScene->Update(Time::GetDeltaTime());

	if (thumbnailLoadInfo.type == Type::MAT || thumbnailLoadInfo.type == Type::MESH)
	{
		UpdateMatMeshThumbnail(thumbnailLoadInfo);
	}
	else if(thumbnailLoadInfo.type == Type::SCENE || thumbnailLoadInfo.type == Type::PREFAB)
	{
		UpdateScenePrefabThumbnail(thumbnailLoadInfo);
	}
	else if (thumbnailLoadInfo.type == Type::TEXTURE)
	{
		UpdateTextureThumbnail(thumbnailLoadInfo);
	}
}

void Editor::Thumbnails::UpdateMatMeshThumbnail(const ThumbnailLoadInfo& thumbnailLoadInfo)
{
	const std::string name = "Thumbnail";

	std::map<std::shared_ptr<Scene>, std::vector<Renderer::RenderViewportInfo>> viewportsByScene;
	const std::shared_ptr<Entity> camera = m_ThumbnailScene->FindEntityByUUID(m_CameraUUID);
	Transform& cameraTransform = camera->GetComponent<Transform>();
	Camera& cameraComponent = camera->GetComponent<Camera>();
	const std::shared_ptr<Scene> scene = camera->GetScene();

	std::shared_ptr<Material> material = nullptr;
	std::shared_ptr<Mesh> mesh = nullptr;
	if (thumbnailLoadInfo.type == Type::MAT)
	{
		material = MaterialManager::GetInstance().LoadMaterial(thumbnailLoadInfo.resourceFilepath);

		// Check if material is skinned, then render default sphere, because no information about the mesh.
		if (Utils::Contains(Utils::ToLower(material->GetBaseMaterial()->GetName()), "skinned"))
		{
			material = nullptr;
		}

		cameraTransform.Translate({ 0.0f, 0.0f, 2.0f });
		cameraTransform.Rotate({});
		cameraComponent.SetZNear(100.0f * 0.001f);
		cameraComponent.SetZFar(100.0f);
	}
	else if (thumbnailLoadInfo.type == Type::MESH)
	{
		mesh = MeshManager::GetInstance().LoadMesh(thumbnailLoadInfo.resourceFilepath);

		BoundingBox bb = mesh->GetBoundingBox();

		glm::vec3 max = bb.max - bb.offset;
		glm::vec3 min = bb.offset - bb.min;

		float maxDistance = 0.0f;
		for (size_t i = 0; i < 3; i++)
		{
			maxDistance = glm::max<float>(maxDistance, max[i]);
			maxDistance = glm::max<float>(maxDistance, glm::abs<float>(min[i]));
		}

		const float distanceScale = 1.3f;
		glm::vec3 cameraPosition = glm::vec3(maxDistance * distanceScale) + bb.offset;
		const glm::mat4 transformMat4 = glm::inverse(glm::lookAt(cameraPosition, bb.offset, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 cameraRotation;
		Utils::DecomposeRotation(transformMat4, cameraRotation);

		cameraTransform.Translate(cameraPosition);
		cameraTransform.Rotate({ cameraRotation.x, cameraRotation.y, 0.0f });

		cameraComponent.SetZNear(maxDistance * distanceScale * 3.0f * 0.001f);
		cameraComponent.SetZFar(maxDistance * distanceScale * 3.0f);
	}

	if (scene != m_ThumbnailScene)
	{
		FATAL_ERROR("Scene for thumbnail is invalid, it is different from default editor thumbnail scene!");
	}

	const std::shared_ptr<Entity> entity = scene->FindEntityByName("Entity");
	auto& r3d = entity->GetComponent<Renderer3D>();

	r3d.mesh = mesh;
	r3d.material = material;

	if (!r3d.mesh)
	{
		r3d.mesh = MeshManager::GetInstance().LoadMesh(std::filesystem::path("Meshes") / "Sphere.mesh");
	}

	if (!r3d.material)
	{
		if (r3d.mesh && r3d.mesh->GetType() == Mesh::Type::SKINNED)
		{
			entity->AddComponent<SkeletalAnimator>();
		}
		
		r3d.material = MaterialManager::GetInstance().LoadMaterial(std::filesystem::path("Materials") / "MeshBaseDoubleSided.mat");
	}

	// Update BVH just in case.
	scene->GetBVH()->Update(SceneBVH::BuildNodes(scene->GetRegistry()));

	Renderer::RenderViewportInfo renderViewportInfo{};
	renderViewportInfo.camera = camera;
	renderViewportInfo.renderView = cameraComponent.GetRendererTarget(name);
	renderViewportInfo.size = m_ThumbnailWindow->GetSize();

	const float aspect = (float)renderViewportInfo.size.x / (float)renderViewportInfo.size.y;
	renderViewportInfo.projection = glm::perspectiveRH_ZO(cameraComponent.GetFov(), aspect, cameraComponent.GetZFar(), cameraComponent.GetZNear());

	viewportsByScene[scene].emplace_back(renderViewportInfo);

	void* frame = m_ThumbnailWindow->BeginFrame();

	m_ThumbnailRenderer->Update(frame, m_ThumbnailWindow, m_ThumbnailRenderer, viewportsByScene);

	m_ThumbnailWindow->EndFrame(frame);

	Pengine::GlobalDataAccessor::GetInstance().GetDevice()->WaitIdle();

	cameraComponent.TakeScreenshot(thumbnailLoadInfo.thumbnailFilepath, name, &m_GeneratingThumbnails.at(thumbnailLoadInfo.resourceFilepath));
	
	r3d.mesh = nullptr;
	r3d.material = nullptr;

	if (mesh)
	{
		if (mesh->GetType() == Mesh::Type::SKINNED)
		{
			entity->RemoveComponent<SkeletalAnimator>();
		}

		MeshManager::GetInstance().DeleteMesh(mesh);
	}

	if (material)
	{
		MaterialManager::GetInstance().DeleteMaterial(material);
	}
}

void Editor::Thumbnails::UpdateScenePrefabThumbnail(const ThumbnailLoadInfo& thumbnailLoadInfo)
{
	const std::string name = "Thumbnail";

	std::shared_ptr<Scene> scene = nullptr;
	std::shared_ptr<Entity> prefab = nullptr;

	if (thumbnailLoadInfo.type == Type::SCENE)
	{
		scene = thumbnailLoadInfo.scene;
		if (!scene)
		{
			Logger::Error("Can't generate scene thumbnail. Scene " + thumbnailLoadInfo.resourceFilepath.string() + " is nullptr!");
		}
	}
	else if (thumbnailLoadInfo.type == Type::PREFAB)
	{
		scene = m_ThumbnailScene;
		prefab = Serializer::DeserializePrefab(thumbnailLoadInfo.resourceFilepath, scene);
		prefab->GetComponent<Transform>().Translate({ 0.0f, 0.0f, 0.0f });
		scene->FindEntityByName("Entity")->SetEnabled(false);
	}

	// Wait until all resources are loaded.
	AsyncAssetLoader::GetInstance().WaitIdle();

	auto camera = scene->CreateEntity("Camera");
	auto& cameraTransform = camera->AddComponent<Transform>(camera);
	auto& cameraComponent = camera->AddComponent<Camera>(camera);
	cameraComponent.CreateRenderView(name, m_ThumbnailWindow->GetSize());

	// Update BVH just in case.
	scene->GetBVH()->Update(SceneBVH::BuildNodes(scene->GetRegistry()));
	
	std::optional<SceneBVH::BVHNode> root = scene->GetBVH()->GetRoot();
	
	BoundingBox bb{};
	if (root)
	{
		bb.max = root->aabb.max;
		bb.min = root->aabb.min;
		bb.offset = bb.max + (bb.min - bb.max) * 0.5f;
	}

	{
		glm::vec3 max = bb.max - bb.offset;
		glm::vec3 min = bb.offset - bb.min;

		float maxDistance = 0.0f;
		for (size_t i = 0; i < 3; i++)
		{
			maxDistance = glm::max<float>(maxDistance, max[i]);
			maxDistance = glm::max<float>(maxDistance, glm::abs<float>(min[i]));
		}

		const float distanceScale = 1.3f;
		glm::vec3 cameraPosition = glm::vec3(maxDistance * distanceScale) + bb.offset;
		const glm::mat4 transformMat4 = glm::inverse(glm::lookAt(cameraPosition, bb.offset, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 cameraRotation;
		Utils::DecomposeRotation(transformMat4, cameraRotation);

		cameraTransform.Translate(cameraPosition);
		cameraTransform.Rotate({ cameraRotation.x, cameraRotation.y, 0.0f });

		cameraComponent.SetZNear(maxDistance * distanceScale * 3.0f * 0.001f);
		cameraComponent.SetZFar(maxDistance * distanceScale * 3.0f);
	}
	auto& globalDataAccessor = GlobalDataAccessor::GetInstance();
	uint32_t& frameInFlightCount = globalDataAccessor.GetFrameInFlightCount();
	uint32_t& frameInFlightIndex = globalDataAccessor.GetFrameInFlightIndex();

	uint32_t previousFrameInFlightIndex = frameInFlightIndex;

	// Need to render n times to initialize every render target and etc.
	// Though this is a scene thumbnail generation, which happen only on save scene action
	// and to this point everything should be already initialized,
	// but still let's just render a couple more frames.
	for (size_t i = 0; i < frameInFlightCount + 1; i++)
	{
		// SetCamera and other functions in Renderer::Update send callbacks to create render target
		// and other resources on the next frame,
		// but we need it now, so we explicitly process events now.
		EventSystem::GetInstance().ProcessEvents();

		std::map<std::shared_ptr<Scene>, std::vector<Renderer::RenderViewportInfo>> viewportsByScene;

		Renderer::RenderViewportInfo renderViewportInfo{};
		renderViewportInfo.camera = camera;
		renderViewportInfo.renderView = cameraComponent.GetRendererTarget(name);
		renderViewportInfo.size = m_ThumbnailWindow->GetSize();

		const float aspect = (float)renderViewportInfo.size.x / (float)renderViewportInfo.size.y;
		renderViewportInfo.projection = glm::perspectiveRH_ZO(cameraComponent.GetFov(), aspect, cameraComponent.GetZFar(), cameraComponent.GetZNear());

		viewportsByScene[scene].emplace_back(renderViewportInfo);

		void* frame = m_ThumbnailWindow->BeginFrame();

		m_ThumbnailRenderer->Update(frame, m_ThumbnailWindow, m_ThumbnailRenderer, viewportsByScene);

		m_ThumbnailWindow->EndFrame(frame);

		frameInFlightIndex = ++frameInFlightIndex % frameInFlightCount;
	}

	Pengine::GlobalDataAccessor::GetInstance().GetDevice()->WaitIdle();

	frameInFlightIndex = previousFrameInFlightIndex;

	cameraComponent.TakeScreenshot(thumbnailLoadInfo.thumbnailFilepath, name, &m_GeneratingThumbnails.at(thumbnailLoadInfo.resourceFilepath));

	Pengine::GlobalDataAccessor::GetInstance().GetDevice()->WaitIdle();

	scene->DeleteEntity(camera);

	if (prefab)
	{
		scene->DeleteEntity(prefab);
		scene->FindEntityByName("Entity")->SetEnabled(true);
	}
}

void Editor::Thumbnails::UpdateTextureThumbnail(const ThumbnailLoadInfo& thumbnailLoadInfo)
{
	std::shared_ptr<Texture> srcTexture = TextureManager::GetInstance().Load(thumbnailLoadInfo.resourceFilepath);
	std::shared_ptr<Texture> dstTexture = RenderPassManager::GetInstance().ScaleTexture(srcTexture, { 64, 64 });
	Serializer::SerializeTexture(thumbnailLoadInfo.thumbnailFilepath, dstTexture, &m_GeneratingThumbnails.at(thumbnailLoadInfo.resourceFilepath));
	TextureManager::GetInstance().Delete(srcTexture);
}

ThumbnailAtlas::TileInfo Editor::Thumbnails::GetOrGenerateThumbnail(
	const std::filesystem::path& filepath,
	std::shared_ptr<Scene> scene,
	Type type)
{
	auto foundThumbnail = m_CacheThumbnails.find(filepath);
	if (m_ThumbnailToCheck != foundThumbnail && foundThumbnail != m_CacheThumbnails.end())
	{
		auto tileInfo = foundThumbnail->second;
		if (tileInfo.atlasIndex >= 0 && tileInfo.tileIndex >= 0)
		{
			return tileInfo;
		}
		else
		{
			m_CacheThumbnails.erase(foundThumbnail);
			m_ThumbnailToCheck = m_CacheThumbnails.end();
		}
	}

	auto shortFilepath = Utils::GetShortFilepath(filepath);
	const auto uuid = Utils::FindUuid(shortFilepath);
	if (uuid.IsValid())
	{
		const std::string uuidString = uuid.ToString();

		std::filesystem::path thumbnailFilepath = "Thumbnails";
		thumbnailFilepath /= uuidString;
		thumbnailFilepath.concat(FileFormats::Png());

		std::filesystem::path thumbnailMetaFilepath = "Thumbnails";
		thumbnailMetaFilepath /= uuidString;
		thumbnailMetaFilepath.concat(FileFormats::Png());
		thumbnailMetaFilepath.concat(".thumbnail");

		size_t lastWriteTimeResourceCurrent = 0;
		size_t lastWriteTimeResourceSaved = 1;

		if (std::filesystem::exists(filepath))
		{
			lastWriteTimeResourceCurrent = std::filesystem::last_write_time(filepath).time_since_epoch().count();
		}

		if (std::filesystem::exists(thumbnailMetaFilepath))
		{
			lastWriteTimeResourceSaved = Serializer::DeserializeThumbnailMeta(thumbnailMetaFilepath);
		}

		auto found = m_GeneratingThumbnails.find(shortFilepath);
		if (found != m_GeneratingThumbnails.end())
		{
			if (!found->second)
			{
				return {};
			}

			TextureManager::GetInstance().Delete(thumbnailFilepath);
			m_GeneratingThumbnails.erase(shortFilepath);
		}

		if (lastWriteTimeResourceSaved == lastWriteTimeResourceCurrent && std::filesystem::exists(thumbnailFilepath))
		{
			auto& tileInfo = m_CacheThumbnails[filepath];
			if (tileInfo.atlasIndex >= 0 && tileInfo.tileIndex >= 0)
			{
				return tileInfo;
			}

			if (!m_IsThumbnailLoading.load())
			{
				m_IsThumbnailLoading.store(true);
				AsyncAssetLoader::GetInstance().AsyncLoadTexture(thumbnailFilepath, [this, filepath](std::weak_ptr<Texture> texture)
				{
					ThumbnailAtlas::TileInfo tileInfo{};
					const auto atlas = m_ThumbnailAtlas.Push(texture.lock(), tileInfo);
					m_CacheThumbnails[filepath] = tileInfo;
					m_IsThumbnailLoading.store(false);

					auto callback = [texture]()
					{
						TextureManager::GetInstance().Delete(texture.lock()->GetFilepath());
					};

					std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
					EventSystem::GetInstance().SendEvent(event);
				}, false);
			}
		}
		else if (!m_GeneratingThumbnails.contains(shortFilepath))
		{
			auto& tileInfo = m_CacheThumbnails[filepath];
			m_ThumbnailAtlas.Free(tileInfo.atlasIndex, tileInfo.tileIndex);
			tileInfo = {};

			m_GeneratingThumbnails.emplace(shortFilepath, false);

			ThumbnailLoadInfo thumbnailLoadInfo{};
			thumbnailLoadInfo.type = type;
			thumbnailLoadInfo.scene = scene;
			thumbnailLoadInfo.resourceFilepath = shortFilepath;
			thumbnailLoadInfo.thumbnailFilepath = thumbnailFilepath;
			m_ThumbnailQueue.emplace_back(thumbnailLoadInfo);

			Serializer::SerializeThumbnailMeta(thumbnailMetaFilepath, lastWriteTimeResourceCurrent);
		}
	}

	return {};
}

ThumbnailAtlas::TileInfo Editor::Thumbnails::TryGetThumbnail(
	const std::filesystem::path& filepath)
{
	auto foundThumbnail = m_CacheThumbnails.find(filepath);
	if (m_ThumbnailToCheck != foundThumbnail && foundThumbnail != m_CacheThumbnails.end())
	{
		auto tileInfo = foundThumbnail->second;
		if (tileInfo.atlasIndex >= 0 && tileInfo.tileIndex >= 0)
		{
			return tileInfo;
		}
		else
		{
			m_CacheThumbnails.erase(foundThumbnail);
			m_ThumbnailToCheck = m_CacheThumbnails.end();
		}
	}

	auto shortFilepath = Utils::GetShortFilepath(filepath);
	const auto uuid = Utils::FindUuid(shortFilepath);
	if (uuid.IsValid())
	{
		const std::string uuidString = uuid.ToString();

		std::filesystem::path thumbnailFilepath = "Thumbnails";
		thumbnailFilepath /= uuidString;
		thumbnailFilepath.concat(FileFormats::Png());

		auto found = m_GeneratingThumbnails.find(shortFilepath);
		if (found != m_GeneratingThumbnails.end())
		{
			if (!found->second)
			{
				return {};
			}

			TextureManager::GetInstance().Delete(thumbnailFilepath);
			m_GeneratingThumbnails.erase(shortFilepath);
		}

		if (std::filesystem::exists(thumbnailFilepath))
		{
			auto& tileInfo = m_CacheThumbnails[filepath];
			if (tileInfo.atlasIndex >= 0 && tileInfo.tileIndex >= 0)
			{
				return tileInfo;
			}

			if (!m_IsThumbnailLoading.load())
			{
				m_IsThumbnailLoading.store(true);
				AsyncAssetLoader::GetInstance().AsyncLoadTexture(thumbnailFilepath, [this, filepath](std::weak_ptr<Texture> texture)
				{
					ThumbnailAtlas::TileInfo tileInfo{};
					const auto atlas = m_ThumbnailAtlas.Push(texture.lock(), tileInfo);
					m_CacheThumbnails[filepath] = tileInfo;
					m_IsThumbnailLoading.store(false);

					auto callback = [texture]()
					{
						TextureManager::GetInstance().Delete(texture.lock()->GetFilepath());
					};

					std::shared_ptr<NextFrameEvent> event = std::make_shared<NextFrameEvent>(callback, Event::Type::OnNextFrame, this);
					EventSystem::GetInstance().SendEvent(event);
				}, false);
			}
		}
	}

	return {};
}
