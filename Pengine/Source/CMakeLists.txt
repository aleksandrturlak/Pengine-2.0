cmake_minimum_required(VERSION 3.8)

project(Pengine VERSION 1.0)

message("Building Pengine")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MP")

if(MSVC)
	add_compile_options(/bigobj)
endif()

set(COMPONENTS_SOURCES
	Components/Camera.cpp Components/Camera.h
	Components/Decal.cpp Components/Decal.h
	Components/DirectionalLight.h
	Components/EntityAnimator.h
	Components/RigidBody.h
	Components/PointLight.h
	Components/Transform.cpp Components/Transform.h
	Components/Renderer3D.cpp Components/Renderer3D.h
	Components/SkeletalAnimator.cpp Components/SkeletalAnimator.h
	Components/SpotLight.h
	Components/Canvas.h
)
source_group("Components" FILES ${COMPONENTS_SOURCES})

set(SYSTEMS_SOURCES
	ComponentSystems/ComponentSystem.h
	ComponentSystems/EntityAnimatorSystem.cpp ComponentSystems/EntityAnimatorSystem.h
	ComponentSystems/PhysicsSystem.cpp ComponentSystems/PhysicsSystem.h
	ComponentSystems/SkeletalAnimatorSystem.cpp ComponentSystems/SkeletalAnimatorSystem.h
	ComponentSystems/UISystem.cpp ComponentSystems/UISystem.h
)
source_group("ComponentSystems" FILES ${SYSTEMS_SOURCES})

set(CORE_SOURCES
	Core/Application.h
	Core/Asset.h
	Core/AsyncAssetLoader.cpp Core/AsyncAssetLoader.h
	Core/BindlessUniformWriter.cpp Core/BindlessUniformWriter.h
	Core/BoundingBox.h
	Core/ClayManager.cpp Core/ClayManager.h
	Core/ColoredOutput.h
	Core/Core.h Core/Core.cpp
	Core/CustomData.h
	Core/CSMRenderer.cpp Core/CSMRenderer.h
	Core/Entity.cpp Core/Entity.h
	Core/EntryPoint.cpp Core/EntryPoint.h
	Core/FileFormatNames.h
	Core/FontManager.cpp Core/FontManager.h
	Core/FrustumCulling.cpp Core/FrustumCulling.h
	Core/GraphicsSettings.h
	Core/Input.cpp Core/Input.h
	Core/KeyCode.h
	Core/LineRenderer.cpp Core/LineRenderer.h
	Core/Logger.cpp Core/Logger.h
	Core/MaterialManager.cpp Core/MaterialManager.h
	Core/MeshManager.cpp Core/MeshManager.h
	Core/NativeHandle.h
	Core/Profiler.h
	Core/RandomGenerator.h
	Core/Raycast.cpp Core/Raycast.h
	Core/ReflectionSystem.cpp Core/ReflectionSystem.h
	Core/RenderPassManager.cpp Core/RenderPassManager.h
	Core/RenderPassOrder.h
	Core/Scene.cpp Core/Scene.h
	Core/SceneBVH.cpp Core/SceneBVH.h
	Core/SceneManager.cpp Core/SceneManager.h
	Core/Serializer.cpp Core/Serializer.h
	Core/SSAORenderer.cpp Core/SSAORenderer.h
	Core/TextureManager.cpp Core/TextureManager.h
	Core/ThreadPool.cpp Core/ThreadPool.h
	Core/Time.cpp Core/Time.h
	Core/Timer.cpp Core/Timer.h
	Core/UUID.cpp Core/UUID.h
	Core/UIRenderer.cpp Core/UIRenderer.h
	Core/Viewport.cpp Core/Viewport.h
	Core/ViewportManager.cpp Core/ViewportManager.h
	Core/Visualizer.cpp Core/Visualizer.h
	Core/Window.cpp Core/Window.h
	Core/WindowManager.cpp Core/WindowManager.h
)
source_group("Core" FILES ${CORE_SOURCES})

set(GRAPHICS_SOURCES
	Graphics/BaseMaterial.cpp Graphics/BaseMaterial.h
	Graphics/Buffer.cpp Graphics/Buffer.h
	Graphics/Device.cpp Graphics/Device.h
	Graphics/Format.h
	Graphics/FrameBuffer.cpp Graphics/FrameBuffer.h
	Graphics/Mesh.cpp Graphics/Mesh.h
	Graphics/MeshBVH.cpp Graphics/MeshBVH.h
	Graphics/Material.cpp Graphics/Material.h
	Graphics/ComputePass.cpp Graphics/ComputePass.h
	Graphics/ComputePipeline.cpp Graphics/ComputePipeline.h
	Graphics/GraphicsPipeline.cpp Graphics/GraphicsPipeline.h
	Graphics/Pass.cpp Graphics/Pass.h
	Graphics/Pipeline.cpp Graphics/Pipeline.h
	Graphics/Renderer.cpp Graphics/Renderer.h
	Graphics/RenderPass.cpp Graphics/RenderPass.h
	Graphics/RenderView.cpp Graphics/RenderView.h
	Graphics/ShaderModule.cpp Graphics/ShaderModule.h
	Graphics/ShaderModuleManager.cpp Graphics/ShaderModuleManager.h
	Graphics/ShaderReflection.h
	Graphics/SkeletalAnimation.cpp Graphics/SkeletalAnimation.h
	Graphics/Skeleton.h
	Graphics/Texture.cpp Graphics/Texture.h
	Graphics/UniformLayout.cpp Graphics/UniformLayout.h
	Graphics/UniformWriter.cpp Graphics/UniformWriter.h
	Graphics/Vertex.h
	Graphics/WriterBufferHelper.h
)
source_group("Graphics" FILES ${GRAPHICS_SOURCES})

set(VULKAN_SOURCES
	Vulkan/ShaderIncluder.cpp Vulkan/ShaderIncluder.h
	Vulkan/VulkanBuffer.cpp Vulkan/VulkanBuffer.h
	Vulkan/VulkanComputePipeline.cpp Vulkan/VulkanComputePipeline.h
	Vulkan/VulkanGraphicsPipeline.cpp Vulkan/VulkanGraphicsPipeline.h
	Vulkan/VulkanPipelineUtils.cpp Vulkan/VulkanPipelineUtils.h
	Vulkan/VulkanDescriptors.cpp Vulkan/VulkanDescriptors.h
	Vulkan/VulkanDevice.cpp Vulkan/VulkanDevice.h
	Vulkan/VulkanFormat.h
	Vulkan/VulkanFrameInfo.h
	Vulkan/VulkanFrameBuffer.cpp Vulkan/VulkanFrameBuffer.h
	Vulkan/VulkanRenderer.cpp Vulkan/VulkanRenderer.h
	Vulkan/VulkanRenderPass.cpp Vulkan/VulkanRenderPass.h
	Vulkan/VulkanSamplerManager.cpp Vulkan/VulkanSamplerManager.h
	Vulkan/VulkanShaderModule.cpp Vulkan/VulkanShaderModule.h
	Vulkan/VulkanTexture.cpp Vulkan/VulkanTexture.h
	Vulkan/VulkanUniformLayout.cpp Vulkan/VulkanUniformLayout.h
	Vulkan/VulkanUniformWriter.cpp Vulkan/VulkanUniformWriter.h
	Vulkan/VulkanWindow.cpp Vulkan/VulkanWindow.h
	Vulkan/VulkanHeadlessWindow.cpp Vulkan/VulkanHeadlessWindow.h
)
source_group("Vulkan" FILES ${VULKAN_SOURCES})

set(SPIRV_REFLECT_SOURCES
	$ENV{VULKAN_SPIRV_REFLECT_PATH}/SPIRV-Reflect/spirv_reflect.c
	$ENV{VULKAN_SPIRV_REFLECT_PATH}/SPIRV-Reflect/spirv_reflect.h
)
source_group("SpirvReflect" FILES ${SPIRV_REFLECT_SOURCES})

set(UTILS_SOURCES
	Utils/Utils.h
)
source_group("Utils" FILES ${UTILS_SOURCES})

set(CONFIGS_SOURCES
	Configs/EngineConfig.h
)
source_group("Configs" FILES ${CONFIGS_SOURCES})

set(EVENT_SYSTEM_SOURCES
	EventSystem/Event.h
	EventSystem/EventSystem.cpp EventSystem/EventSystem.h
	EventSystem/NextFrameEvent.h
	EventSystem/ResizeEvent.h
)
source_group("EventSystem" FILES ${EVENT_SYSTEM_SOURCES})

add_library(${PROJECT_NAME} SHARED
	${COMPONENTS_SOURCES}
	${CORE_SOURCES}
	${GRAPHICS_SOURCES}
	${VULKAN_SOURCES}
	${SPIRV_REFLECT_SOURCES}
	${SYSTEMS_SOURCES}
	${UTILS_SOURCES}
	${CONFIGS_SOURCES}
	${EVENT_SYSTEM_SOURCES}
	${EDITOR_SOURCES})

target_compile_definitions(${PROJECT_NAME} PUBLIC PENGINE_ENGINE=1)

target_link_libraries(${PROJECT_NAME} PRIVATE YAML)
target_link_libraries(${PROJECT_NAME} PRIVATE ImGui)
target_link_libraries(${PROJECT_NAME} PRIVATE freetype)
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf)
target_link_libraries(${PROJECT_NAME} PRIVATE meshoptimizer)
target_link_libraries(${PROJECT_NAME} PRIVATE Jolt)

if (WIN32)
	if (MSVC)
		find_library(GLFW_LIBRARY NAMES glfw glfw3 HINTS $ENV{GLFW_LIB_PATH} REQUIRED)
	elseif (MINGW)
		find_library(GLFW_LIBRARY NAMES glfw3dll HINTS $ENV{GLFW_LIB_PATH} REQUIRED)
	endif ()
elseif (UNIX)
	find_library(GLFW_LIBRARY NAMES glfw glfw3 HINTS $ENV{GLFW_LIB_PATH} REQUIRED)
endif ()

if (GLFW_LIBRARY)
	message(${GLFW_LIBRARY} " has been found")
	target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARY})
else ()
	message(FATAL_ERROR "GLFW library has not been found")
endif ()

find_library(VULKAN_LIBRARY NAMES vulkan vulkan-1 HINTS $ENV{VULKAN_LIB_PATH} REQUIRED)
if (VULKAN_LIBRARY)
	message(${VULKAN_LIBRARY} " has been found")
	target_link_libraries(${PROJECT_NAME} PRIVATE ${VULKAN_LIBRARY})
else ()
	message(FATAL_ERROR "Vulkan library has not been found")
endif ()

if (WIN32)
	if (MSVC)
		find_library(SHADERC_COMBINED_R_LIBRARY NAMES shaderc_combined HINTS $ENV{VULKAN_LIB_PATH} REQUIRED)
		find_library(SHADERC_COMBINED_D_LIBRARY NAMES shaderc_combinedd HINTS $ENV{VULKAN_LIB_PATH} REQUIRED)
	elseif (MINGW)
		find_library(SHADERC_COMBINED_R_LIBRARY NAMES shaderc_shared HINTS $ENV{VULKAN_LIB_PATH} REQUIRED)
		find_library(SHADERC_COMBINED_D_LIBRARY NAMES shaderc_sharedd HINTS $ENV{VULKAN_LIB_PATH} REQUIRED)
	endif ()

	if (SHADERC_COMBINED_R_LIBRARY)
		message(${SHADERC_COMBINED_R_LIBRARY} " has been found")
	else ()
		message(FATAL_ERROR "Shaderc combined library release has not been found")
	endif ()

	if (SHADERC_COMBINED_D_LIBRARY)
		message(${SHADERC_COMBINED_D_LIBRARY} " has been found")
	else ()
		message(FATAL_ERROR "Shaderc combined library debug has not been found")
	endif ()

	target_link_libraries(${PROJECT_NAME} PRIVATE debug ${SHADERC_COMBINED_D_LIBRARY} optimized ${SHADERC_COMBINED_R_LIBRARY})
elseif (UNIX)
	find_library(SHADERC_LIBRARY NAMES libshaderc_combined.a HINTS $ENV{VULKAN_LIB_PATH} REQUIRED)
	if (SHADERC_LIBRARY)
		message(${SHADERC_LIBRARY} " has been found")
	else ()
		message("Shaderc combined has not been found")
	endif ()
	target_link_libraries(${PROJECT_NAME} PRIVATE ${SHADERC_LIBRARY})
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/yaml-cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/imgui)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/fastgltf/include)
target_include_directories(${PROJECT_NAME} PRIVATE ../../Vendor/JoltPhysics)

target_include_directories(${PROJECT_NAME} PRIVATE $ENV{GLFW_INCLUDE_PATH})
target_include_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_INCLUDE_PATH})
target_include_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SPIRV_REFLECT_PATH})

if (GENERATOR_IS_MULTI_CONFIG)
	add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/Vendor/freetype/Build/$<CONFIG>
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/$<CONFIG>)

	add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/Vendor/fastgltf/Build/$<CONFIG>
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/$<CONFIG>)

	add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/Vendor/JoltPhysics/Build/$<CONFIG>
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/$<CONFIG>)

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/$<CONFIG>
		${CMAKE_BINARY_DIR}/${APP_NAME}/Build/$<CONFIG>)

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build/$<CONFIG>
		${CMAKE_BINARY_DIR}/Editor/Build/$<CONFIG>)
else ()
	add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/Vendor/freetype/Build
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build)

	add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/Vendor/fastgltf/Build
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build)

	add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/Vendor/JoltPhysics/Build
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build)

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build
		${CMAKE_BINARY_DIR}/${APP_NAME}/Build)

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_BINARY_DIR}/${PROJECT_NAME}/Build
		${CMAKE_BINARY_DIR}/Editor/Build)
endif ()
