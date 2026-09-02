cmake_minimum_required(VERSION 3.15)
project(AstralEngine VERSION 1.0.0 LANGUAGES CXX)

# --- Global Ayarlar ---
# Modern C++20 standartlarini zorunlu kil (ECS sablonlari icin gerekli)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# VS Code / clangd otomatik tamamlama icin
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows icin platforma ozel ayarlar
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- 1. Motorun Cekirdek Kutuphanesi (AstralCore) ---
add_library(AstralCore
    src/Core/Application.cpp
)

# Baslik dosyalarinin yolunu belirt (PUBLIC yaparak Sandbox'in da gormesini sagliyoruz)
target_include_directories(AstralCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Motorun kendisini isimlendir
set_target_properties(AstralCore PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# --- 2. Ana Test (Sandbox) Uygulamasi ---
add_executable(Sandbox
    src/main.cpp
)

# Motoru test projesine bagla
target_link_libraries(Sandbox PRIVATE AstralCore)

# --- Ileriye Donuk Hazirlik Notlari ---
# Vulkan: third_party/vulkan/ icine SDK konulduktan sonra:
#   find_package(Vulkan REQUIRED)
#   target_link_libraries(AstralCore PUBLIC Vulkan::Vulkan)
#
# GLFW: third_party/glfw/ icine kaynak konduktan sonra:
#   add_subdirectory(third_party/glfw)
#   target_link_libraries(AstralCore PUBLIC glfw)
#
# EnTT (ECS): third_party/entt/ icine header-only koyulduktan sonra:
#   target_include_directories(AstralCore PUBLIC third_party/entt)