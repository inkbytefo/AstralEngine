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

# Vulkan SDK
find_package(Vulkan REQUIRED)

# --- 1. Motorun Cekirdek Kutuphanesi (AstralCore) ---
add_library(AstralCore
    src/Core/Application.cpp
    src/Core/Window.cpp
    src/Renderer/VulkanContext.cpp
)

target_include_directories(AstralCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(AstralCore PUBLIC
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

target_link_libraries(AstralCore PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
)

set_target_properties(AstralCore PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# --- 2. Ana Test (Sandbox) Uygulamasi ---
add_executable(Sandbox
    src/main.cpp
)

target_link_libraries(Sandbox PRIVATE AstralCore)