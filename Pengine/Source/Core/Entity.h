#pragma once

#include "Core.h"

namespace Pengine
{

	class Scene;

	class PENGINE_API Entity : public std::enable_shared_from_this<Entity>
	{
	public:
		explicit Entity(
			std::shared_ptr<Scene> scene,
			std::string name = "Unnamed",
			UUID uuid = UUID());
		Entity(const Entity& entity);
		Entity(Entity&& entity) noexcept;
		~Entity();

		entt::registry& GetRegistry() const;

		std::shared_ptr<Scene> GetScene() const;

		entt::entity GetHandle() const { return m_Handle; }

		std::size_t operator()() const { return static_cast<size_t>(m_Handle); }

		bool operator==(const Entity& entity) const { return m_Handle == entity.m_Handle; }

		Entity& operator=(const Entity& entity);

		Entity& operator=(Entity&& entity) noexcept;

		template<typename T, typename ...Args>
		T& AddComponent(Args&&... args)
		{
			return m_Registry->emplace<T>(m_Handle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent() const
		{
			return m_Registry->get<T>(m_Handle);
		}

		template<typename T>
		bool HasComponent() const
		{
			return m_Registry->any_of<T>(m_Handle);
		}

		template<typename T>
		void RemoveComponent()
		{
			NotifySceneAboutComponentRemove(std::string(GetTypeName<T>()));
			m_Registry->remove<T>(m_Handle);
		}

		bool HasParent() const { return GetParent() != nullptr && m_Handle != entt::tombstone; }

		void SetParent(const std::shared_ptr<Entity>& parent) { m_Parent = parent; }

		/**
		 * Get the root parent of the hierarchy.
		 * If there is no parent then return itself.
		 */
		std::shared_ptr<Entity> GetTopEntity();

		std::shared_ptr<Entity> GetParent() const { return m_Parent.lock(); }

		std::shared_ptr<Entity> FindEntityInHierarchy(const std::string& name);

		void AddChild(const std::shared_ptr<Entity>& child, const bool saveTransform = true);

		void RemoveChild(const std::shared_ptr<Entity>& child);

		bool HasAsChild(const std::shared_ptr<Entity>& child, bool recursevely = false);

		bool HasAsParent(const std::shared_ptr<Entity>& parent, bool recursevely = false);

		const std::vector<std::weak_ptr<Entity>>& GetChilds() const { return m_Childs; }

		const std::string& GetName() const { return m_Name; }

		void SetName(const std::string& name) { m_Name = name; }

		const UUID& GetUUID() const { return m_UUID; }

		void SetUUID(const UUID& uuid);

		bool IsEnabled() const;

		void SetEnabled(const bool isEnabled);

		void SetPrefabFilepathUUID(const UUID& prefabFilepathUUID) { m_PrefabFilepathUUID = prefabFilepathUUID; }
		
		const UUID& GetPrefabFilepathUUID() const { return m_PrefabFilepathUUID; }

		bool IsPrefab() const { return m_PrefabFilepathUUID.IsValid(); }

		bool IsValid() const { return !IsDeleted(); }
		
		bool IsDeleted() const { return m_IsDeleted; }
		
		void SetDeleted(const bool isDeleted);

		void NotifySceneAboutComponentRemove(const std::string& componentName);

	private:
		void Copy(const Entity& entity);

		void Move(Entity&& entity) noexcept;

		void SetEnabledDirty();

		entt::entity m_Handle{entt::tombstone};
		std::weak_ptr<Entity> m_Parent;
		std::vector<std::weak_ptr<Entity>> m_Childs;
		std::vector<entt::entity> m_ChildEntities;
		std::weak_ptr<Scene> m_Scene;
		entt::registry* m_Registry = nullptr;
		std::string m_Name;
		UUID m_UUID;

		UUID m_PrefabFilepathUUID = UUID(0, 0);

		bool m_IsEnabled = true;
		bool m_IsDeleted = false;

		mutable bool m_IsEnabledInternal = true;
		mutable bool m_IsEnabledDirty = true;

		friend class Scene;
	};

}

template<>
struct std::hash<Pengine::Entity>
{
	std::size_t operator()(const Pengine::Entity& entity) const noexcept
	{
		return static_cast<size_t>(entity.GetHandle());
	}
};
