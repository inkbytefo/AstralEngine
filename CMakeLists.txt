cmake_minimum_required(VERSION 3.20)
project(AstralEngine VERSION 1.0.0 LANGUAGES CXX)

# --- Global Ayarlar ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows / MSVC ve GCC icin platforma ozel ayarlar
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Harici Bagimliliklar (FetchContent & Vulkan) ---
include(FetchContent)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b-docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# ImGuizmo (3D Transform Manipulator)
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
)
target_include_directories(imgui_lib PUBLIC 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${imguizmo_SOURCE_DIR}/src
)
target_compile_definitions(imgui_lib PRIVATE VK_NO_PROTOTYPES)
target_compile_definitions(imgui_lib PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_link_libraries(imgui_lib PUBLIC Vulkan::Vulkan glfw)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# --- Shader Derleme (glslc -> SPIR-V) ---
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders/SDFCompute.glsl")
set(SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/SDFCompute.spv")

add_custom_command(
    OUTPUT ${SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=compute ${SHADER_SRC} -o ${SHADER_SPV}
    DEPENDS ${SHADER_SRC}
    COMMENT "Compiling ${SHADER_SRC} -> ${SHADER_SPV}"
)

set(TAA_SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders/TAAResolve.glsl")
set(TAA_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/TAAResolve.spv")

add_custom_command(
    OUTPUT ${TAA_SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=compute ${TAA_SHADER_SRC} -o ${TAA_SHADER_SPV}
    DEPENDS ${TAA_SHADER_SRC}
    COMMENT "Compiling ${TAA_SHADER_SRC} -> ${TAA_SHADER_SPV}"
)

add_custom_target(CompileShaders ALL DEPENDS ${SHADER_SPV} ${TAA_SHADER_SPV})

# --- 1. Motorun Cekirdek Kutuphanesi (AstralCore) ---
add_library(AstralCore
    src/Core/Application.cpp
    src/Core/Window.cpp
    src/Core/InputSystem.cpp
    src/Core/Systems/InputSubsystem.cpp
    src/Core/Systems/PhysicsSubsystem.cpp
    src/Core/Systems/TransformSubsystem.cpp
    src/Core/Systems/RenderExtractionSubsystem.cpp
    src/Core/TransformSystem.cpp
    src/Core/BenchmarkLogger.cpp
    src/Core/RenderExtractionSystem.cpp
    src/Renderer/VulkanContext.cpp
    src/Renderer/ComputePipeline.cpp
    src/Renderer/Buffer.cpp
    src/Renderer/BrickGrid.cpp
    src/Renderer/SDFRenderer.cpp
    src/Renderer/Swapchain.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneSerializer.cpp
    src/Editor/EditorUI.cpp
    src/Editor/EditorTheme.cpp
    src/Editor/EditorWorkspace.cpp
    src/Editor/EditorMenuBar.cpp
    src/Editor/EditorStatusBar.cpp
    src/Editor/Gizmo/TransformGizmo.cpp
    src/Editor/Gizmo/ViewportGizmoToolbar.cpp
    src/Editor/Panels/SceneHierarchy.cpp
    src/Editor/Panels/Inspector.cpp
    src/Editor/Panels/Statistics.cpp
    src/Editor/Panels/ViewportPanel.cpp
    src/Editor/Panels/ContentBrowser.cpp
)

add_dependencies(AstralCore CompileShaders)

target_include_directories(AstralCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(AstralCore PUBLIC
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    SHADER_BIN_DIR="${CMAKE_BINARY_DIR}/shaders"
)

target_link_libraries(AstralCore PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
    imgui_lib
)

set_target_properties(AstralCore PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# --- 2. Ana Test (Sandbox) Uygulamasi ---
add_executable(Sandbox
    src/main.cpp
)

target_link_libraries(Sandbox PRIVATE AstralCore)
