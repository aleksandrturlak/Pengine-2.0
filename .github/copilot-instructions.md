# Pengine 2.0 AI Coding Instructions

## Architecture Overview
Pengine is a C++23 3D game engine using Vulkan for rendering, EnTT for ECS, and YAML for serialization. Core components:
- **ECS Structure**: Entities wrap `entt::entity` with scene context. Components (e.g., `Transform`, `Renderer3D`) use templates for type-safe access.
- **Rendering Pipeline**: Deferred rendering with PBR, shadows (CSM/point/spot), SSAO, SSR, SSS, bloom, FXAA, tone mapping. Vulkan backend abstracts graphics objects.
- **Asset Management**: Async loading with managers for textures, meshes, materials. Materials are scriptable with options/uniforms based on base materials.
- **Serialization**: YAML-based with UUIDs for all assets/files. `.meta` files store metadata alongside assets.
- **Physics**: Jolt Physics integrated via `PhysicsSystem` and `RigidBody` component.

## Key Conventions
- **File Structure**: Headers and sources paired (e.g., `Transform.h`/`Transform.cpp`). Use `#pragma once` and `PENGINE_API` macro for shared library exports.
- **Naming**: PascalCase for classes/types, camelCase for methods/variables. Prefix with `m_` for members, `s_` for statics.
- **Math**: GLM for vectors/matrices. `Transform` uses dirty flags for efficient matrix updates (local/global separation).
- **Shaders**: GLSL with `.meta` UUID files. Includes shared code from `Shaders/Includes/`. Compiled via Vulkan SDK's shaderc.
- **Error Handling**: Use `Logger::Error()` for exceptions. Prefer assertions and early returns over exceptions in hot paths.
- **Memory**: Shared pointers for assets/entities. RAII for Vulkan resources via wrapper classes.

## Build & Run Workflow
- **Setup**: Set Vulkan/GLFW env vars (e.g., `VULKAN_INCLUDE_PATH`, `GLFW_LIB_PATH`). Clone with `--recursive`.
- **Build**: `mkdir Build && cd Build && cmake .. && cmake --build .` (Ninja generator). Pengine builds as shared lib, SandBox/Editor as executables.
- **Run**: Set working directory to `SandBox/` or `Editor/`. Launch SandBox for runtime, Editor for level design.
- **Debug**: Use VS Code debugger with working dir set. Logs in `Log.txt` files.

## Testing
- Unit tests in `Test/` use GTest. Uncomment `add_subdirectory(Test Test/Build)` in root CMakeLists.txt to enable.
- Run: `cd Build && ctest` after building.

## Common Patterns
- **Component Access**: `entity->AddComponent<Transform>(args...)`, `entity->GetComponent<Transform>()`.
- **Asset Loading**: `Material::Load(filepath)` returns shared_ptr. Use `AsyncAssetLoader` for background loading.
- **Event System**: `EventSystem::GetInstance().SendEvent<ResizeEvent>(width, height)` for decoupled communication.
- **Rendering**: Create `GraphicsPipeline` from YAML config, bind via `Renderer`. Use `UniformWriter` for shader data.
- **Scene Management**: `SceneManager::GetInstance().Create(name, tag)` for scenes. Entities updated via systems in `Scene::Update()`.

## Integration Points
- **Vulkan**: Direct via `VulkanDevice`, `VulkanRenderer`. SPIRV-Reflect for shader introspection.
- **ImGui**: For UI in Editor/SandBox. Integrated via `UISystem`.
- **Jolt Physics**: Bodies created in `PhysicsSystem::Update()`. Raycasts via `Raycast` utility.
- **External Libs**: FastGLTF for model loading, FreeType for fonts, MeshOptimizer for LODs.

Reference: `Pengine/Source/Core/Scene.h` for ECS patterns, `Pengine/Source/Graphics/Material.h` for asset scripting, `SandBox/Shaders/` for GLSL examples.</content>
<parameter name="filePath">/home/alexander/Repositories/Pengine-2.0/.github/copilot-instructions.md