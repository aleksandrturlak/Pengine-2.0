cmake_minimum_required(VERSION 3.8)

project(Editor VERSION 1.0)

message("Building Editor")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MP")

set(CORE_SOURCES
	Core/Editor.cpp Core/Editor.h
    Core/EditorApplication.cpp Core/EditorApplication.h
	Core/EntityAnimatorEditor.cpp Core/EntityAnimatorEditor.h
	Core/ImGuizmo.cpp Core/ImGuizmo.h
	Core/ThumbnailAtlas.cpp Core/ThumbnailAtlas.h
    Core/main.cpp
)
source_group("Core" FILES ${CORE_SOURCES})

file(GLOB_RECURSE COMPONENT_HEADER_FILES "../../SandBox/Source/**/*.h")
list(FILTER COMPONENT_HEADER_FILES INCLUDE REGEX "[Cc]omponent")

set(MASTER_HEADER_CONTENT "// Auto-generated master include file\n// Do not edit manually!\n\n")
set(MASTER_HEADER_CONTENT "${MASTER_HEADER_CONTENT}#pragma once\n\n")

foreach (headerFile ${COMPONENT_HEADER_FILES})
    get_filename_component(FILENAME ${headerFile} NAME)
    if (NOT FILENAME STREQUAL "Components.h")
        set(MASTER_HEADER_CONTENT "${MASTER_HEADER_CONTENT}#include \"${headerFile}\"\n")
    endif ()
endforeach ()

file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/Include/Components.h" "${MASTER_HEADER_CONTENT}")

file(GLOB_RECURSE ALL_SANDBOX_CPP "../../SandBox/Source/**/*.cpp")
set(COMPONENT_REGISTRATION_FILES "")
foreach(cpp_file ${ALL_SANDBOX_CPP})
    file(STRINGS ${cpp_file} _matched REGEX "// Component Registration\\.")
    if(_matched)
        list(APPEND COMPONENT_REGISTRATION_FILES ${cpp_file})
    endif()
endforeach()

list(LENGTH COMPONENT_REGISTRATION_FILES _reg_count)
if(_reg_count GREATER 1)
    message(FATAL_ERROR "Only one component registration file per project is allowed. "
        "Found ${_reg_count} files: ${COMPONENT_REGISTRATION_FILES}")
endif()

if(_reg_count EQUAL 1)
    list(GET COMPONENT_REGISTRATION_FILES 0 COMPONENT_REGISTRATION_FILE)
    message("Component registration file: ${COMPONENT_REGISTRATION_FILE}")
endif()

add_executable(${PROJECT_NAME} ${CORE_SOURCES} ${COMPONENT_REGISTRATION_FILE})

target_compile_definitions(${PROJECT_NAME} PUBLIC PENGINE_ENGINE=0)

target_link_libraries(${PROJECT_NAME} PRIVATE Jolt)
target_link_libraries(${PROJECT_NAME} PRIVATE YAML)
target_link_libraries(${PROJECT_NAME} PRIVATE ImGui)
target_link_libraries(${PROJECT_NAME} PRIVATE Pengine)

target_include_directories(${PROJECT_NAME} PRIVATE $ENV{GLFW_INCLUDE_PATH})
target_include_directories(${PROJECT_NAME} PRIVATE ../../Pengine/Source)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/fastgltf/include)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/JoltPhysics)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/imgui)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor)

if (MSVC)
	set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${APP_NAME}")
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
