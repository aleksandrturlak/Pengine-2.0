#pragma once

#pragma warning(disable : 4715)
#pragma warning(disable : 4251)
#pragma warning(disable : 4267)
#pragma warning(disable : 4244)
#pragma warning(disable : 26812)
#pragma warning(disable : 26451)
#define CRT_OBSOLETE_NO_WARNINGS

#ifdef _WIN32
	#ifdef PENGINE_ENGINE
		#define PENGINE_API __declspec(dllexport)
	#else
		#define PENGINE_API __declspec(dllimport)
	#endif
#elif __linux__
	#ifdef PENGINE_ENGINE
		#define PENGINE_API __attribute__((visibility("default")))
	#else
		#define PENGINE_API
	#endif
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

#include <entt/src/entt/entt.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "UUID.h"
#include "NativeHandle.h"

// Render pass constants
#define MAX_PIPELINE_COUNT_PER_MATERIAL 8
#define MAX_PIPELINE_COUNT 128

#define RTTR_CAT_IMPL(a, b) a##b
#define RTTR_CAT(a, b) RTTR_CAT_IMPL(a, b)

#define RTTR_REGISTRATION_USER_DEFINED(type)                                                                           \
	static void rttr_auto_register_reflection_function_##type();                                                       \
	namespace                                                                                                          \
	{                                                                                                                  \
		struct rttr__auto__register__##type                                                                            \
		{                                                                                                              \
			rttr__auto__register__##type()                                                                             \
			{                                                                                                          \
				rttr_auto_register_reflection_function_##type();                                                       \
			}                                                                                                          \
		};                                                                                                             \
	}                                                                                                                  \
	static const rttr__auto__register__##type RTTR_CAT(RTTR_CAT(auto_register__, __LINE__), type);                     \
	static void rttr_auto_register_reflection_function_##type()

inline constexpr char const* none = "None";
inline constexpr char const* plane = "Plane";

inline constexpr glm::vec3 topLevelRenderPassDebugColor = { 0.5f, 1.0f, 0.5f };

struct path_hash
{
	std::size_t operator()(const std::filesystem::path& path) const
	{
		return hash_value(path);
	}
};

template<typename T>
inline constexpr const char* GetTypeName()
{
	return entt::type_name<std::remove_cv_t<std::remove_reference_t<T>>>().value().data();
}

template<typename T>
inline constexpr entt::id_type GetTypeHash()
{
	return entt::type_hash<std::remove_cv_t<std::remove_reference_t<T>>>().value();
}

enum class MemoryType
{
	CPU,
	GPU
};

enum class GraphicsAPI
{
	Software,
	Ogl,
	Vk
};

inline GraphicsAPI graphicsAPI;

namespace Pengine
{
	inline std::mutex uuidMutex;
	inline std::unordered_map<UUID, std::filesystem::path, uuid_hash> filepathByUuid;
	inline std::unordered_map<std::filesystem::path, UUID, path_hash> uuidByFilepath;

	// TODO: Maybe move this somewhere!
	inline int drawCallCount;
	inline size_t triangleCount;
	inline size_t currentFrame = 0;
	inline int64_t vramAllocated = 0;

	inline std::shared_ptr<class Device> device = nullptr;

	inline glm::mat4 identityMat4 = glm::mat4(1.0f);

	namespace Vk
	{
		inline uint32_t swapChainImageCount = 0;
		inline uint32_t swapChainImageIndex = 0;
	}

	class PENGINE_API GlobalDataAccessor
	{
	public:
		static GlobalDataAccessor& GetInstance();

		GlobalDataAccessor(const GlobalDataAccessor&) = delete;
		GlobalDataAccessor& operator=(const GlobalDataAccessor&) = delete;

		std::unordered_map<UUID, std::filesystem::path, uuid_hash>& GetFilepathByUuid();
		std::unordered_map<std::filesystem::path, UUID, path_hash>& GetUuidByFilepath();

		int GetDrawCallCount() const;
		size_t GetTriangleCount() const;
		size_t GetCurrentFrame() const;
		int64_t GetVramAllocated() const;

		uint32_t& GetSwapChainImageCount();
		uint32_t& GetSwapChainImageIndex();

		std::mutex& GetUUIDMutex();

		std::shared_ptr<class Device> GetDevice() const;

	private:
		GlobalDataAccessor() = default;
		~GlobalDataAccessor() = default;
	};
}
