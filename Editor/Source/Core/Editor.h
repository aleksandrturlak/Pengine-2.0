#pragma once

#include "ThumbnailAtlas.h"

#include "Core/Core.h"
#include "Core/GraphicsSettings.h"
#include "Core/Serializer.h"
#include "Graphics/Texture.h"

#include "EntityAnimatorEditor.h"

#include <deque>

namespace Pengine
{
	class Scene;
	class Entity;
	class Window;
	class BaseMaterial;
	class Material;
	class Mesh;
	class Renderer;
}

class Editor
{
public:
	Editor();
	~Editor();

	void Update(const std::shared_ptr<Pengine::Scene>& scene, Pengine::Window& window);

	enum class RuntimeState
	{
		PLAY,
		STOP
	};

	RuntimeState GetRuntimeState() const { return m_RuntimeState; }

private:
	class Indent
	{
	public:
		Indent();
		~Indent();
	};

	bool DrawAngle3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f,
		const glm::vec2& limits = glm::vec2(-360.0f, 360.0f), float columnWidth = 100.0f) const;

	bool DrawVec2Control(const std::string& label, glm::vec2& values, float resetValue = 0.0f,
		const glm::vec2& limits = glm::vec2(-25.0f, 25.0f), float speed = 0.1f, float columnWidth = 100.0f) const;

	bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f,
		const glm::vec2& limits = glm::vec2(-25.0f, 25.0f), float speed = 0.1f, float columnWidth = 100.0f) const;

	bool DrawVec4Control(const std::string& label, glm::vec4& values, float resetValue = 0.0f,
		const glm::vec2& limits = glm::vec2(-25.0f, 25.0f), float speed = 0.1f, float columnWidth = 100.0f) const;

	bool DrawIVec2Control(const std::string& label, glm::ivec2& values, float resetValue = 0.0f,
		const glm::vec2& limits = glm::vec2(-25.0f, 25.0f), float speed = 0.1f, float columnWidth = 100.0f) const;

	bool DrawIVec3Control(const std::string& label, glm::ivec3& values, float resetValue = 0.0f,
		const glm::vec2& limits = glm::vec2(-25.0f, 25.0f), float speed = 0.1f, float columnWidth = 100.0f) const;

	bool DrawIVec4Control(const std::string& label, glm::ivec4& values, float resetValue = 0.0f,
		const glm::vec2& limits = glm::vec2(-25.0f, 25.0f), float speed = 0.1f, float columnWidth = 100.0f) const;

	bool ImageCheckBox(const void* id, ImTextureID textureOn, ImTextureID textureOff, bool& enabled);

	void Hierarchy(const std::shared_ptr<Pengine::Scene>& scene, Pengine::Window& window);

	void SceneInfo(const std::shared_ptr<Pengine::Scene>& scene);

	void DrawScene(const std::shared_ptr<Pengine::Scene>& scene, Pengine::Window& window);

	void DrawNode(const std::shared_ptr<Pengine::Entity>& entity, ImGuiTreeNodeFlags flags, Pengine::Window& window);

	void DrawChilds(const std::shared_ptr<Pengine::Entity>& entity, Pengine::Window& window);

	void Properties(const std::shared_ptr<Pengine::Scene>& scene, Pengine::Window& window);

	void GraphicsSettingsInfo(Pengine::GraphicsSettings& graphicsSettings);

	void CameraComponent(const std::shared_ptr<Pengine::Entity>& entity, Pengine::Window& window);

	void TransformComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void Renderer3DComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void PointLightComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void SpotLightComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void DirectionalLightComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void SkeletalAnimatorComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void EntityAnimatorComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void CanvasComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void PhysicsBoxComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void DecalComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void AudioSourceComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void AudioListenerComponent(const std::shared_ptr<Pengine::Entity>& entity);

	void UserComponents(const std::shared_ptr<Pengine::Entity>& entity);

	void GameObjectPopUpMenu(const std::shared_ptr<Pengine::Scene>& scene);

	void ComponentsPopUpMenu(const std::shared_ptr<Pengine::Entity>& entity);

	void MainMenuBar(const std::shared_ptr<Pengine::Scene>& scene);

	void AssetBrowser(const std::shared_ptr<Pengine::Scene>& scene);

	void AssetBrowserHierarchy();

	void DrawAssetBrowserHierarchy(const std::filesystem::path& directory);

	void SetDarkThemeColors();

	void Manipulate(const std::shared_ptr<Pengine::Scene>& scene, Pengine::Window& window);

	void MoveCamera(const std::shared_ptr<Pengine::Entity>& camera, Pengine::Window& window);

	void SaveScene(std::shared_ptr<Pengine::Scene> scene);

	void PlayButtonMenu(std::shared_ptr<Pengine::Scene> scene);

	void DrawBitMask(const std::string& label, void* bitMask, size_t bitCount, size_t maxRows);

	struct MaterialMenu
	{
		std::shared_ptr<Pengine::Material> material = nullptr;
		bool opened = false;

		void Update(Editor& editor);
	} m_MaterialMenu;

	struct BaseMaterialMenu
	{
		std::shared_ptr<Pengine::BaseMaterial> baseMaterial = nullptr;
		bool opened = false;

		void Update(const Editor& editor);
	} m_BaseMaterialMenu;

	struct MeshMenu
	{
		std::shared_ptr<Pengine::Mesh> mesh = nullptr;
		bool opened = false;

		void Update(const Editor& editor);
	} m_MeshMenu;

	struct CloneMaterialMenu
	{
		char name[64];

		bool opened = false;

		std::shared_ptr<Pengine::Material> material = nullptr;

		void Update();
	} m_CloneMaterialMenu;

	struct CreateFileMenu
	{
		char name[64];

		bool opened = false;

		std::filesystem::path filepath;
		std::string format;

		void Update();

		static void MakeFile(const std::filesystem::path& filepath);
	} m_CreateFileMenu;

	struct DeleteFileMenu
	{
		bool opened = false;

		std::filesystem::path filepath;

		void Update();
	} m_DeleteFileMenu;

	struct CreateViewportMenu
	{
		bool opened = false;

		char name[64];
		glm::ivec2 size = { 1024, 1024 };

		void Update(const Editor& editor, Pengine::Window& window);
	} m_CreateViewportMenu;

	struct LoadIntermediateMenu
	{
		bool opened = false;

		std::string workName = none;
		float workStatus = 0.0f;

		void Update();
	} m_LoadIntermediateMenu;

	struct TextureMetaPropertiesMenu
	{
		bool opened = false;

		Pengine::Texture::Meta meta{};

		void Update();
	} m_TextureMetaPropertiesMenu;

	struct ImportMenu
	{
		bool opened = false;

		Pengine::Serializer::ImportInfo importInfo;
		Pengine::Serializer::ImportOptions importOptions;

		void Update(Editor& editor);
	} m_ImportMenu;

	std::filesystem::path m_RootDirectory = std::filesystem::current_path();
	std::filesystem::path m_CurrentDirectory = m_RootDirectory;

	char m_AssetBrowserFilterBuffer[64];

	float m_ThumbnailScale = 1.0f;

	int m_TransformSystem = 0;

	bool m_DrawVecLabel = true;

	bool m_FullScreen = false;

	std::shared_ptr<Pengine::Entity> m_MovingCamera;

	class Thumbnails
	{
	public:
		std::shared_ptr<Pengine::Scene> m_ThumbnailScene;
		std::shared_ptr<Pengine::Window> m_ThumbnailWindow;
		std::shared_ptr<Pengine::Renderer> m_ThumbnailRenderer;
		Pengine::UUID m_CameraUUID;

		std::atomic<bool> m_IsThumbnailLoading = false;

		Pengine::ThumbnailAtlas m_ThumbnailAtlas;

		enum class Type
		{
			MESH,
			MAT,
			SCENE,
			PREFAB,
			TEXTURE
		};

		struct ThumbnailLoadInfo
		{
			std::filesystem::path thumbnailFilepath;
			std::filesystem::path resourceFilepath;
			std::shared_ptr<Pengine::Scene> scene;
			Type type;
		};
		std::deque<ThumbnailLoadInfo> m_ThumbnailQueue;
		std::unordered_map<std::filesystem::path, bool> m_GeneratingThumbnails;
		std::unordered_map<std::filesystem::path, Pengine::ThumbnailAtlas::TileInfo> m_CacheThumbnails;
		std::unordered_map<std::filesystem::path, Pengine::ThumbnailAtlas::TileInfo>::iterator m_ThumbnailToCheck = m_CacheThumbnails.end();

		void Initialize();

		void ShutDown();

		void UpdateThumbnails();

		void UpdateMatMeshThumbnail(const ThumbnailLoadInfo& thumbnailLoadInfo);

		void UpdateScenePrefabThumbnail(const ThumbnailLoadInfo& thumbnailLoadInfo);

		void UpdateTextureThumbnail(const ThumbnailLoadInfo& thumbnailLoadInfo);

		Pengine::ThumbnailAtlas::TileInfo GetOrGenerateThumbnail(
			const std::filesystem::path& filepath,
			std::shared_ptr<Pengine::Scene> scene,
			Type type);

		Pengine::ThumbnailAtlas::TileInfo TryGetThumbnail(const std::filesystem::path& filepath);
	} m_Thumbnails;

	Pengine::ThumbnailAtlas::TileInfo GetFileIcon(const std::filesystem::path& filepath, const std::string& format);

	EntityAnimatorEditor m_EntityAnimatorEditor;
	bool m_EntityAnimatorEditorOpened = false;

	struct CopyTransform
	{
		glm::vec3 position{ 0.0f, 0.0f, 0.0f };
		glm::vec3 rotation{ 0.0f, 0.0f, 0.0f };
		glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
	} m_CopyTransform;

	RuntimeState m_RuntimeState = RuntimeState::STOP;

	bool RunProcess(const std::filesystem::path& executable, const std::filesystem::path& workingDir, const std::string& arguments = "");

	bool isSnapEnabled = false;
	float snap = 0.5f;

	friend class EntityAnimatorEditor;
};
