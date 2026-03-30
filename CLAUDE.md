# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Pengine 2.0 is a C++23 3D game engine using Vulkan for rendering. It targets Windows and Linux.

## Build

Requires environment variables pointing to Vulkan/GLFW SDKs:
- Linux: `VULKAN_SPIRV_REFLECT_PATH`, `VULKAN_INCLUDE_PATH`, `VULKAN_LIB_PATH`, `VULKAN_BIN_PATH`
- Windows: additionally `GLFW_INCLUDE_PATH`, `GLFW_LIB_PATH`

```bash
mkdir Build && cd Build && cmake ..
cmake --build .
```

Pengine builds as a shared library. SandBox and Editor build as executables. Set working directory to `SandBox/` or `Editor/` when running.

Logs output to `SandBox/Log.txt`. Performance traces to `SandBox/Trace.json`.

## Tests

Unit tests in `Test/` use GTest. They are commented out by default — uncomment `add_subdirectory(Test Test/Build)` in the root [CMakeLists.txt](CMakeLists.txt) to enable.

```bash
cd Build && ctest
```

## Architecture

**Depth** The engine uses reversed depth.

**ECS:** EnTT-based. Entities wrap `entt::entity` with scene context. Access components via `entity->AddComponent<T>(args...)` / `entity->GetComponent<T>()`. See [Pengine/Source/Core/Scene.h](Pengine/Source/Core/Scene.h) for patterns.

**Rendering:** Deferred rendering pipeline with PBR, CSM shadow maps, SSAO, SSR, SSS, bloom, FXAA, and tone mapping. The `RenderPassManager` orchestrates render pass execution order. `GraphicsPipeline` instances are created from YAML configs. `UniformWriter` binds shader data. See [Pengine/Source/Core/RenderPassManager.h](Pengine/Source/Core/RenderPassManager.h).

**Vulkan Layer:** `VulkanDevice` and `VulkanRenderer` provide direct Vulkan access. RAII wrappers for all Vulkan resources. SPIRV-Reflect handles shader introspection. Shaders are GLSL compiled to SPIR-V via shaderc; shared includes live in [SandBox/Shaders/Includes/](SandBox/Shaders/Includes/).

**Asset Management:** Managers for textures, meshes, materials. Use `Material::Load(filepath)` returning `shared_ptr`. `AsyncAssetLoader` handles background loading. All assets have UUID-based `.meta` files alongside them.

**Serialization:** YAML-based with UUIDs for all assets/files. See [Pengine/Source/Core/Serializer.h](Pengine/Source/Core/Serializer.h).

**Events:** `EventSystem::GetInstance().SendEvent<T>(args...)` for decoupled communication.

**Physics:** Jolt Physics via `PhysicsSystem` and `RigidBody` component. Bodies created in `PhysicsSystem::Update()`. Raycasts via the `Raycast` utility.

## Code Conventions

- `#pragma once` for all headers; `PENGINE_API` macro for shared library exports
- PascalCase for classes/types; camelCase for methods/variables
- `m_` prefix for member variables; `s_` prefix for statics
- GLM for all math (vectors, matrices); `Transform` uses dirty flags for local/global matrix separation
- `Logger::Error()` for errors; prefer assertions and early returns over exceptions in hot paths
- Shared pointers for assets and entities; RAII for Vulkan resources

## Game Development
When making a game there is a file in MEMORY how to make a game with this game engine.