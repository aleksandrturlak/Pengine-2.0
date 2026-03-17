#pragma once

#include "Core.h"
#include "Asset.h"
#include "Entity.h"
#include "Visualizer.h"
#include "GraphicsSettings.h"
#include "SceneBVH.h"

#include "../Graphics/RenderView.h"
#include "../ComponentSystems/ComponentSystem.h"
#include "../ComponentSystems/PhysicsSystem.h"

namespace Pengine
{

	class PENGINE_API Scene : public Asset, public std::enable_shared_from_this<Scene>
	{
	public:
		struct Settings
		{
			bool drawBoundingBoxes = false;
			bool drawPhysicsShapes = false;
		};

		static std::shared_ptr<Scene> Create(const std::string& name, const std::string& tag);

		Scene(const std::string& name, const std::filesystem::path& filepath);
		Scene(const Scene& scene);
		~Scene();
		Scene& operator=(const Scene& scene);

		void Update(const float deltaTime);

		void UpdateSystems(const float deltaTime);

		std::shared_ptr<Entity> CreateEntity(const std::string& name = "Unnamed", const UUID& uuid = UUID());

		std::shared_ptr<Entity> CloneEntity(std::shared_ptr<Entity> entity);

		// Delete the entity in the next frame, the entity will be marked deleted,
		// which can be checked by calling entity->IsEnabled() or entity->IsDeleted() or entity->IsValid().
		void DeleteEntity(std::shared_ptr<Entity>& entity);

		std::shared_ptr<Entity> FindEntityByUUID(const UUID& uuid);

		std::shared_ptr<Entity> FindEntityByName(const std::string& name);

		const std::vector<std::shared_ptr<Entity>>& GetEntities() const { return m_Entities; }

		/**
		 * Entity can change uuid internally, so we need to replace uuid in the search map as well.
		 */
		void ReplaceEntityUUID(UUID oldUUID, UUID newUUID);

		void Clear();

		void SetTag(const std::string& tag) { m_Tag = tag; }

		void SetFilepath(const std::filesystem::path& filepath) { m_Filepath = filepath; }

		std::string GetTag() const { return m_Tag; }

		entt::registry& GetRegistry() { return m_Registry; }

		Visualizer& GetVisualizer() { return m_Visualizer; }

		Settings& GetSettings() { return m_Settings; }

		GraphicsSettings& GetGraphicsSettings() { return m_GraphicsSettings; }

		void SetGraphicsSettings(const GraphicsSettings& graphicsSettings) { m_GraphicsSettings = graphicsSettings; }

		std::set<std::shared_ptr<Entity>>& GetSelectedEntities() { return m_SelectedEntities; }

		std::shared_ptr<RenderView> GetRenderView() const { return m_RenderView; }

		std::shared_ptr<SceneBVH> GetBVH() const { return m_CurrentBVH; }

		void SetRenderView(std::shared_ptr<RenderView> renderView) { m_RenderView = renderView; }

		void SetComponentSystem(const std::string& name, std::function<std::shared_ptr<ComponentSystem>()> componentSystem) { m_ComponentSystemsByName[name] = componentSystem(); }

		void ProcessComponentRemove(const std::string& componentName, std::shared_ptr<Entity> entity) const;

		std::shared_ptr<ComponentSystem> GetComponentSystem(const std::string& name);

		std::shared_ptr<PhysicsSystem> GetPhysicsSystem() const { return m_PhysicsSystem; }

		void UpdateTLAS(const std::vector<AccelerationStructure::Instance>& instances, void* frame);

		std::shared_ptr<AccelerationStructure> GetTLAS() const { return m_TLAS[Vk::frameInFlightIndex]; }

		std::shared_ptr<Entity> CreateEmpty();

		std::shared_ptr<Entity> CreateCamera();

		std::shared_ptr<Entity> CreateDirectionalLight();

		std::shared_ptr<Entity> CreatePointLight();

		std::shared_ptr<Entity> CreateSpotLight();

		std::shared_ptr<Entity> CreateCube();

		std::shared_ptr<Entity> CreateSphere();

		std::shared_ptr<Entity> CreateCanvas();

		struct WindSettings
		{
			glm::vec3 direction = { -1.0f, 0.0f, 0.0f };
			float strength = 1.0f;
			float frequency = 1.0f;
		};

		const WindSettings& GetWindSettings() { return m_WindSettings; }

		void SetWindSettings(const WindSettings& windSettings) { m_WindSettings = windSettings; }
	private:
		struct Resources
		{
			std::set<std::shared_ptr<class Mesh>> meshes;
			std::set<std::shared_ptr<class BaseMaterial>> baseMaterials;
			std::set<std::shared_ptr<class Material>> materials;
		};

		std::unordered_map<std::string, std::shared_ptr<ComponentSystem>> m_ComponentSystemsByName;

		std::unordered_map<UUID, std::shared_ptr<Entity>, uuid_hash> m_EntitiesByUUID;

		std::vector<std::shared_ptr<Entity>> m_Entities;
		std::queue<std::shared_ptr<Entity>> m_EntityDeletionQueue;
		entt::registry m_Registry;
		Visualizer m_Visualizer;
		Settings m_Settings;
		GraphicsSettings m_GraphicsSettings;
		WindSettings m_WindSettings;

		std::string m_Tag = none;

		std::set<std::shared_ptr<Entity>> m_SelectedEntities;

		std::shared_ptr<RenderView> m_RenderView;

		std::shared_ptr<PhysicsSystem> m_PhysicsSystem;
		
		std::shared_ptr<SceneBVH> m_BuildingBVH;
		std::shared_ptr<SceneBVH> m_CurrentBVH;
		bool m_IsBuildingBVH = false;
		bool m_IsSystemUpdating = true;
		std::mutex m_LockBVH;
		std::condition_variable m_BVHConditionalVariable;

		std::vector<std::shared_ptr<AccelerationStructure>> m_TLAS;

		void Copy(const Scene& scene);

		void FlushDeletionQueue();
	};

}
