#pragma once

#include "Core.h"

namespace Pengine
{

	template <typename T>
	inline constexpr bool IsVectorType = false;

	template <typename T, typename A>
	inline constexpr bool IsVectorType<std::vector<T, A>> = true;

	class PENGINE_API ReflectionSystem
	{
	private:
		ReflectionSystem() = default;
		~ReflectionSystem() = default;
		ReflectionSystem(const ReflectionSystem&) = delete;
		ReflectionSystem& operator=(const ReflectionSystem&)
		{
			return *this;
		}

	public:
		class Property
		{
		public:
			std::string m_Type;
			entt::id_type m_TypeId = 0;
			bool m_IsVector = false;
			size_t m_Offset = 0;

			template <typename T> bool IsValue() const
			{
				return m_TypeId == GetTypeHash<T>();
			}

			bool IsVector() const
			{
				return m_IsVector;
			}

			template <typename T> T& GetValue(void* owner, const size_t baseOffset)
			{
				T& value = *reinterpret_cast<T*>(static_cast<char*>(owner) + m_Offset + baseOffset);
				return value;
			}

			template <typename T> const T& GetValue(void* owner, const size_t baseOffset) const
			{
				const T& value = *reinterpret_cast<T*>(static_cast<char*>(owner) + m_Offset + baseOffset);
				return value;
			}

			template <typename T> void SetValue(void* owner, const T& value, const size_t baseOffset)
			{
				GetValue<T>(owner, baseOffset) = value;
			}
		};

		class ParentInitializer
		{
		public:
			ParentInitializer(const entt::id_type& child, const entt::id_type& newParent, size_t offset)
			{
				if (child == newParent)
				{
					return;
				}

				if (const auto classByType = GetInstance().m_ClassesByType.find(child);
					classByType != GetInstance().m_ClassesByType.end())
				{
					if (GetInstance().m_ClassesByType.count(newParent) == 0)
					{
						return;
					}

					for (const auto& [parentName, parentOffset] : classByType->second.m_Parents)
					{
						if (parentName == newParent)
						{
							return;
						}
					}

					classByType->second.m_Parents.push_back({newParent, offset});
				}
			}
		};

		struct RegisteredClass
		{
			std::vector<std::function<void(void*, void*)>> m_CopyPropertyCallBacks;
			std::unordered_map<std::string, Property> m_PropertiesByName;
			std::vector<std::pair<entt::id_type, size_t>> m_Parents;
			std::function<void*(entt::registry&, entt::entity)> m_CreateCallback;
			std::function<void(entt::registry&, entt::entity)> m_RemoveCallback;
			std::function<void(void*, void*, void*)> m_SerializeCallback;
			std::function<void(void*, void*, void*)> m_DeserializeCallback;
			entt::type_info m_TypeInfo;

			RegisteredClass(const entt::type_info& typeInfo) : m_TypeInfo(typeInfo) {}
		};

		std::map<entt::id_type, RegisteredClass> m_ClassesByType;

		static ReflectionSystem& GetInstance();

		static std::pair<Property*, size_t> FindPropertyHelper(const entt::id_type& classId,
			const std::string& propertyName, size_t offset)
		{
			if (const auto classByType = GetInstance().m_ClassesByType.find(classId);
				classByType != GetInstance().m_ClassesByType.end())
			{
				if (const auto it = classByType->second.m_PropertiesByName.find(propertyName);
					it != classByType->second.m_PropertiesByName.end())
				{
					return { &it->second, offset };
				}

				for (const auto& [parentId, parentOffset] : classByType->second.m_Parents)
				{
					auto result = FindPropertyHelper(parentId, propertyName, offset + parentOffset);
					if (result.first)
						return result;
				}
			}

			return { nullptr, 0 };
		}

		template <typename T>
		static bool SetValueImpl(const T& value, void* instance, const entt::id_type& classId,
								 const std::string& propertyName, size_t offset)
		{
			auto [prop, foundOffset] = FindPropertyHelper(classId, propertyName, offset);
			if (prop && prop->IsValue<T>())
			{
				prop->SetValue(instance, value, foundOffset);
				return true;
			}
			return false;
		}

		template <typename T>
		static void SetValue(const T& value, void* instance, const entt::id_type& classId,
							 const std::string& propertyName)
		{
			SetValueImpl(value, instance, classId, propertyName, 0);
		}

		template <typename T>
		static T GetValueImpl(void* instance, const entt::id_type& classId, const std::string& propertyName,
							  size_t offset)
		{
			auto [prop, foundOffset] = FindPropertyHelper(classId, propertyName, offset);
			if (prop && prop->IsValue<T>())
				return prop->GetValue<T>(instance, foundOffset);
			return T{};
		}

		template <typename T>
		static T GetValue(void* instance, const entt::id_type& classId, const std::string& propertyName)
		{
			return GetValueImpl<T>(instance, classId, propertyName, 0);
		}
	};

#define COM ,

#define REGISTER_CLASS(_type)                                                                                          \
	RTTR_REGISTRATION_USER_DEFINED(_type)                                                                              \
	{                                                                                                                  \
		Pengine::ReflectionSystem::RegisteredClass registeredClass(entt::type_id<_type>());                            \
		registeredClass.m_CreateCallback = [](entt::registry& registry, entt::entity entity)                           \
		{                                                                                                              \
			return (void*)&registry.emplace<_type>(entity);                                                            \
		};                                                                                                             \
		registeredClass.m_RemoveCallback = [](entt::registry& registry, entt::entity entity)                           \
		{                                                                                                              \
			registry.remove<_type>(entity);                                                                            \
		};                                                                                                             \
		Pengine::ReflectionSystem::GetInstance().m_ClassesByType.emplace(                                              \
			std::make_pair(GetTypeHash<_type>(), registeredClass));                                                    \
	}

#define SERIALIZE_CALLBACK(_callback)                                                                                  \
	std::function<void(void*, void*, void*)> serializeCallback = [baseClass = this]                                    \
		{                                                                                                              \
			auto classByType =                                                                                         \
				Pengine::ReflectionSystem::GetInstance().m_ClassesByType.find(GetTypeHash<decltype(*baseClass)>());    \
			if (classByType != Pengine::ReflectionSystem::GetInstance().m_ClassesByType.end())                         \
			{                                                                                                          \
				classByType->second.m_SerializeCallback = &_callback;                                                  \
			}                                                                                                          \
			return &_callback;                                                                                         \
		}();

#define DESERIALIZE_CALLBACK(_callback)                                                                                \
private:                                                                                                               \
	std::function<void(void*, void*, void*)> deserializeCallback = [baseClass = this]                                  \
		{                                                                                                              \
			auto classByType =                                                                                         \
				Pengine::ReflectionSystem::GetInstance().m_ClassesByType.find(GetTypeHash<decltype(*baseClass)>());    \
			if (classByType != Pengine::ReflectionSystem::GetInstance().m_ClassesByType.end())                         \
			{                                                                                                          \
				classByType->second.m_DeserializeCallback = &_callback;                                                \
			}                                                                                                          \
			return &_callback;                                                                                         \
		}();

#define PROPERTY(_type, _name, _value)                                                                                 \
	_type _name = [baseClass = this]() -> _type                                                                        \
		{                                                                                                              \
			using ___OwnerType = std::remove_pointer_t<decltype(baseClass)>;                                           \
			auto classByType =                                                                                         \
				Pengine::ReflectionSystem::GetInstance().m_ClassesByType.find(GetTypeHash<___OwnerType>());            \
			if (classByType != Pengine::ReflectionSystem::GetInstance().m_ClassesByType.end())                         \
			{                                                                                                          \
				if (classByType->second.m_PropertiesByName.count(#_name) > 0)                                          \
					return _value;                                                                                     \
				classByType->second.m_PropertiesByName.emplace(                                                        \
					std::string(#_name),                                                                               \
					Pengine::ReflectionSystem::Property{                                                               \
						std::string(GetTypeName<_type>()),                                                             \
						GetTypeHash<_type>(),                                                                          \
						Pengine::IsVectorType<_type>,                                                                  \
						offsetof(___OwnerType, _name)});                                                               \
				classByType->second.m_CopyPropertyCallBacks.push_back(                                                 \
					[=](void* a, void* b)                                                                              \
					{ static_cast<decltype(baseClass)>(a)->_name = static_cast<decltype(baseClass)>(b)->_name; });     \
			}                                                                                                          \
			return _value;                                                                                             \
		}();

#define COPY_PROPERTIES(_component)                                                                                    \
	std::function<void(const entt::id_type&, size_t)> copyProperties =                                                 \
		[this, &_component, &copyProperties](const entt::id_type& typeHash, size_t offset)                             \
	{                                                                                                                  \
		auto classByType = Pengine::ReflectionSystem::GetInstance().m_ClassesByType.find(typeHash);                    \
		if (classByType != Pengine::ReflectionSystem::GetInstance().m_ClassesByType.end())                             \
		{                                                                                                              \
			for (size_t i = 0; i < classByType->second.m_CopyPropertyCallBacks.size(); i++)                            \
			{                                                                                                          \
				classByType->second.m_CopyPropertyCallBacks[i]((char*)this + offset, (char*)&_component + offset);     \
			}                                                                                                          \
		}                                                                                                              \
		for (const auto& parent : classByType->second.m_Parents)                                                       \
		{                                                                                                              \
			copyProperties(parent.first, parent.second);                                                               \
		}                                                                                                              \
	};                                                                                                                 \
	copyProperties(GetTypeHash<decltype(_component)>(), 0);

#define REGISTER_PARENT_CLASS(_type)                                                                                   \
private:                                                                                                               \
	Pengine::ReflectionSystem::ParentInitializer ___ParentInitializer_##_type = Pengine::ReflectionSystem::ParentInitializer(\
		GetTypeHash<decltype(*this)>(), GetTypeHash<_type>(), (size_t)(_type*)(this) - (size_t)this);

}