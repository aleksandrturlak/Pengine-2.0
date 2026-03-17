cmake_minimum_required(VERSION 3.8)

project(Pengine VERSION 1.0)

set(APP_NAME "SandBox")

if (UNIX OR MINGW)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_subdirectory(Vendor/imgui Vendor/imgui/Build)
add_subdirectory(Vendor/yaml-cpp Vendor/yaml-cpp/Build)
add_subdirectory(Vendor/entt Vendor/entt/Build)
add_subdirectory(Vendor/freetype Vendor/freetype/Build)
add_subdirectory(Vendor/fastgltf Vendor/fastgltf/Build)
add_subdirectory(Vendor/meshoptimizer Vendor/meshoptimizer/Build)

add_subdirectory(Vendor/JoltPhysics/Build Vendor/JoltPhysics/Build)

if (MSVC)
    # Set runtime library for the JoltPhysics target
    target_compile_options(Jolt PRIVATE $<$<CONFIG:Debug>:/MDd>)
    target_compile_options(Jolt PRIVATE $<$<CONFIG:Release>:/MD>)
    target_compile_options(Jolt PRIVATE $<$<CONFIG:RelWithDebInfo>:/MD>)
    target_compile_options(Jolt PRIVATE $<$<CONFIG:MinSizeRel>:/MD>)
endif ()

add_subdirectory(Pengine/Source Pengine/Build)

option(BUILD_TESTS "Build unit tests" OFF)
if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(Test Test/Build)
else ()
    add_subdirectory(SandBox/Source SandBox/Build)
    add_subdirectory(Editor/Source Editor/Build)
endif ()