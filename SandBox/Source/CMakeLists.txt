cmake_minimum_required(VERSION 3.8)

project(${APP_NAME} VERSION 1.0)

message("Building SandBox")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MP")

set(CORE_SOURCES
	Core/main.cpp
)
source_group("Core" FILES ${CORE_SOURCES})

set(GAME_SOURCES
	Game/GameApplication.h
	Game/GameApplication.cpp
	Game/PlayerComponent.h
	Game/WeaponComponent.h
	Game/EnemyComponent.h
	Game/PlayerSystem.h
	Game/PlayerSystem.cpp
	Game/WeaponSystem.h
	Game/WeaponSystem.cpp
	Game/ProjectileComponent.h
	Game/ProjectileSystem.h
	Game/ProjectileSystem.cpp
	Game/EnemySystem.h
	Game/EnemySystem.cpp
	Game/WaveComponent.h
	Game/WaveSystem.h
	Game/WaveSystem.cpp
	Game/PickupComponent.h
	Game/PickupSystem.h
	Game/PickupSystem.cpp
	# Roguelike systems
	Game/ItemComponent.h
	Game/InventoryComponent.h
	Game/InteractableComponent.h
	Game/LootContainerComponent.h
	Game/ShopComponent.h
	Game/PatrolComponent.h
	Game/GameStateComponent.h
	Game/RoguelikeState.h
	Game/InventorySystem.h
	Game/InventorySystem.cpp
	Game/LootSystem.h
	Game/LootSystem.cpp
	Game/ShopSystem.h
	Game/ShopSystem.cpp
	Game/InteractableSystem.h
	Game/InteractableSystem.cpp
	Game/PatrolSystem.h
	Game/PatrolSystem.cpp
	Game/GameStateSystem.h
	Game/GameStateSystem.cpp
	Game/ProceduralMapSystem.h
	Game/ProceduralMapSystem.cpp
	Game/ComponentRegistration.cpp
)
source_group("Game" FILES ${GAME_SOURCES})

add_executable(${PROJECT_NAME} ${CORE_SOURCES} ${GAME_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(${PROJECT_NAME} PUBLIC PENGINE_ENGINE=0)

target_link_libraries(${PROJECT_NAME} PRIVATE Jolt)
target_link_libraries(${PROJECT_NAME} PRIVATE YAML)
target_link_libraries(${PROJECT_NAME} PRIVATE Pengine)

target_include_directories(${PROJECT_NAME} PRIVATE $ENV{GLFW_INCLUDE_PATH})
target_include_directories(${PROJECT_NAME} PRIVATE ../../Pengine/Source)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/fastgltf/include)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/JoltPhysics)

if (MSVC)
	set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}")
endif ()

if (WIN32)
	if (MINGW)
		if (GENERATOR_IS_MULTI_CONFIG)
			add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$ENV{GLFW_LIB_PATH}/glfw3.dll
				${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/$<CONFIG>/glfw3.dll)

			add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$ENV{VULKAN_BIN_PATH}/shaderc_shared.dll
				${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/$<CONFIG>/shaderc_shared.dll)

			add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$ENV{VULKAN_BIN_PATH}/shaderc_sharedd.dll
				${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/$<CONFIG>/shaderc_sharedd.dll)
		else ()
			add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$ENV{GLFW_LIB_PATH}/glfw3.dll
				${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/glfw3.dll)

			add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$ENV{VULKAN_BIN_PATH}/shaderc_shared.dll
				${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/shaderc_shared.dll)

			add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$ENV{VULKAN_BIN_PATH}/shaderc_sharedd.dll
				${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/shaderc_sharedd.dll)
		endif ()
	endif()
endif ()
