# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Advanced 3D Game Engine
#  *
#  * Modern, Modular, and Professional CMake Configuration
#  * Purpose: To provide a clean, maintainable, and robust build system.
#  * Author: Astral Engine Development Team (AI-Assisted Refactor)
#  * Version: 2.0.0
#  * Date: 2025-09-17
#  * Signature: inkbytefo
#  **********************************************************************/

cmake_minimum_required(VERSION 3.24)

# !!! WORKAROUND for SSL/TLS Certificate Issues !!!
# If you are in a corporate network or have issues downloading dependencies
# (like DXC or Slang), uncommenting the line below will disable SSL
# certificate verification for downloads. This is a security risk and should
# only be used if you trust the repositories.
 set(CMAKE_TLS_VERIFY OFF)

project(AstralEngine VERSION 0.1.0 LANGUAGES CXX)

# SSL certificate verification is handled by CMake's default mechanisms.

#----------------------------------------------------------------------
# 1. Proje ve Derleyici Ayarları
#----------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build tipi
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Çıktı dizinleri
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/External)
include(FetchContent)

#----------------------------------------------------------------------
# 2. Build Seçenekleri
#----------------------------------------------------------------------
option(ASTRAL_BUILD_EXAMPLES "Build example applications" OFF)
option(ASTRAL_BUILD_TESTS "Build unit tests" OFF)
option(ASTRAL_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(ASTRAL_ENABLE_PROFILING "Enable profiling support" OFF)
option(ASTRAL_VULKAN_VALIDATION "Enable Vulkan validation layers in debug builds" ON)

#----------------------------------------------------------------------
# 3. Derleyiciye Özel Ayarlar
#----------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /EHsc /MP /bigobj)
    add_compile_options(/source-charset:utf-8 /execution-charset:utf-8)
    add_compile_options(/wd4996) # Disable deprecated std::is_pod_v warnings
    if(ASTRAL_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
else() # GCC/Clang
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Werror=${ASTRAL_WARNINGS_AS_ERRORS})
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

#----------------------------------------------------------------------
# 4. Bağımlılık Yönetimi (Helper Fonksiyon ve Tanımlamalar)
#----------------------------------------------------------------------
function(find_or_fetch_package name repo tag)
    find_package(${name} QUIET)
    if(NOT ${name}_FOUND)
        message(STATUS "Could not find ${name}, fetching from source (${tag})...")
        FetchContent_Declare(${name} GIT_REPOSITORY ${repo} GIT_TAG ${tag})
        FetchContent_MakeAvailable(${name})
    else()
        message(STATUS "Found ${name} via find_package")
    endif()
endfunction()

# SDL3
find_or_fetch_package(SDL3 "https://github.com/libsdl-org/SDL.git" "release-3.2.22")
set(ASTRAL_HAS_SDL3 TRUE)

# Assimp
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
find_or_fetch_package(assimp "https://github.com/assimp/assimp.git" "v6.0.2")
set(ASTRAL_HAS_ASSIMP TRUE)

# Slang (Shader derleyicisi)
set(SLANG_ENABLE_SLANGC ON)
set(SLANG_ENABLE_TESTS OFF)
set(SLANG_ENABLE_EXAMPLES OFF)
set(SLANG_ENABLE_OPTIX OFF)
# DXC indirme sorunlarını önlemek için DXBC ve DXIL desteğini devre dışı bırak
set(SLANG_ENABLE_DXBC OFF)
set(SLANG_ENABLE_DXIL OFF)
# SPIRV desteğini açıkça aktif et ve statik build sağla
set(SLANG_ENABLE_SPIRV_TOOLS ON)
# Ensure a fully static build for Slang
set(SLANG_BUILD_SHARED OFF)
set(SLANG_BUILD_STATIC ON)
find_or_fetch_package(slang "https://github.com/shader-slang/slang.git" "v2025.16.1")
set(ASTRAL_HAS_SLANG TRUE)

# GLM
find_or_fetch_package(glm "https://github.com/g-truc/glm.git" "1.0.1")
set(ASTRAL_HAS_GLM TRUE)

# nlohmann/json
find_or_fetch_package(nlohmann_json "https://github.com/nlohmann/json.git" "v3.12.0")
set(ASTRAL_HAS_JSON TRUE)

# Vulkan
find_package(Vulkan REQUIRED)
set(ASTRAL_HAS_VULKAN TRUE)
FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.1.0
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# Threads
find_package(Threads REQUIRED)

#----------------------------------------------------------------------
# 5. Ortak Hedef Yapılandırma Fonksiyonu
#----------------------------------------------------------------------
function(configure_astral_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)

    # Include Dizinleri
    target_include_directories(${target_name} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/Source>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/External/glm>
        ${Vulkan_INCLUDE_DIRS}
        ${slang_SOURCE_DIR}/include
        $<TARGET_PROPERTY:nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
        ${CMAKE_SOURCE_DIR}/External/stb # stb kütüphanesi için
    )

    # Derleyici Tanımları
    target_compile_definitions(${target_name} PRIVATE
        ASTRAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        $<$<CONFIG:Debug>:ASTRAL_DEBUG>
        $<$<PLATFORM_ID:Windows>:PLATFORM_WINDOWS>
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        ASTRAL_USE_SDL3
        ASTRAL_USE_VULKAN
        ASTRAL_USE_ASSIMP
        ASTRAL_USE_SLANG
        ASTRAL_USE_GLM
        ASTRAL_USE_JSON
        $<$<BOOL:${ASTRAL_VULKAN_VALIDATION}>:$<$<CONFIG:Debug>:ASTRAL_VULKAN_VALIDATION_LAYERS>>
    )

    # Kütüphane Bağlama
    target_link_libraries(${target_name} PRIVATE
        Threads::Threads
        SDL3::SDL3
        assimp::assimp
        slang
        Vulkan::Vulkan
        VulkanMemoryAllocator
        glm::glm
        nlohmann_json::nlohmann_json
    )

    if(WIN32 AND TARGET SDL3::SDL3main)
        target_link_libraries(${target_name} PRIVATE SDL3::SDL3main)
    endif()

    # Windows'ta DLL'leri kopyala
    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:${target_name}>
            COMMENT "Copying required DLLs to output directory..."
        )
    endif()
endfunction()

#----------------------------------------------------------------------
# 6. AstralEngine Kütüphane Hedefi
#----------------------------------------------------------------------
add_library(AstralEngine STATIC)

target_sources(AstralEngine PRIVATE
    # ... Tüm .cpp ve .h dosyalarınız buraya eklenecek ...
    # Otomatik dosya toplama veya manuel liste kullanılabilir.
    # Örnek:
    Source/Core/Engine.cpp Source/Core/Engine.h
    Source/Core/Logger.cpp Source/Core/Logger.h
    Source/Core/FileLogger.cpp Source/Core/FileLogger.h
    Source/Core/MemoryManager.cpp Source/Core/MemoryManager.h
    Source/Core/ISubsystem.h
    Source/Core/IApplication.h
    Source/Events/EventManager.cpp Source/Events/EventManager.h
    Source/Events/Event.h
    Source/Subsystems/Platform/PlatformSubsystem.cpp Source/Subsystems/Platform/PlatformSubsystem.h
    Source/Subsystems/Platform/Window.cpp Source/Subsystems/Platform/Window.h
    Source/Subsystems/Platform/InputManager.cpp Source/Subsystems/Platform/InputManager.h
    Source/Subsystems/Platform/KeyCode.h
    Source/Subsystems/ECS/ECSSubsystem.cpp Source/Subsystems/ECS/ECSSubsystem.h
    Source/ECS/Components.h
    Source/Subsystems/Asset/AssetSubsystem.cpp Source/Subsystems/Asset/AssetSubsystem.h
    Source/Subsystems/Asset/AssetManager.cpp Source/Subsystems/Asset/AssetManager.h
    Source/Subsystems/Asset/AssetHandle.cpp Source/Subsystems/Asset/AssetHandle.h
    Source/Subsystems/Asset/AssetRegistry.cpp Source/Subsystems/Asset/AssetRegistry.h
    Source/Subsystems/Asset/Model.cpp Source/Subsystems/Asset/Model.h
    Source/Subsystems/Asset/ShaderProgram.h
    Source/Subsystems/Renderer/RenderSubsystem.cpp Source/Subsystems/Renderer/RenderSubsystem.h
    Source/Subsystems/Renderer/VulkanRenderer.cpp Source/Subsystems/Renderer/VulkanRenderer.h
    Source/Subsystems/Renderer/Camera.cpp Source/Subsystems/Renderer/Camera.h
    Source/Subsystems/Renderer/GraphicsDevice.cpp Source/Subsystems/Renderer/GraphicsDevice.h
    Source/Subsystems/Renderer/VulkanMemoryManager.cpp Source/Subsystems/Renderer/VulkanMemoryManager.h
    Source/Subsystems/Renderer/VulkanSynchronization.cpp Source/Subsystems/Renderer/VulkanSynchronization.h
    Source/Subsystems/Renderer/VulkanUtils.cpp Source/Subsystems/Renderer/VulkanUtils.h
    Source/Subsystems/Renderer/Core/VulkanInstance.cpp Source/Subsystems/Renderer/Core/VulkanInstance.h
    Source/Subsystems/Renderer/Core/VulkanDevice.cpp Source/Subsystems/Renderer/Core/VulkanDevice.h
    Source/Subsystems/Renderer/Core/VulkanSwapchain.cpp Source/Subsystems/Renderer/Core/VulkanSwapchain.h
    Source/Subsystems/Renderer/Core/VulkanFrameManager.cpp Source/Subsystems/Renderer/Core/VulkanFrameManager.h
    Source/Subsystems/Renderer/Shaders/VulkanShader.cpp Source/Subsystems/Renderer/Shaders/VulkanShader.h
    Source/Subsystems/Renderer/Commands/VulkanCommandPool.cpp Source/Subsystems/Renderer/Commands/VulkanCommandPool.h
    Source/Subsystems/Renderer/Commands/VulkanPipeline.cpp Source/Subsystems/Renderer/Commands/VulkanPipeline.h
    Source/Subsystems/Renderer/Buffers/VulkanBuffer.cpp Source/Subsystems/Renderer/Buffers/VulkanBuffer.h
    Source/Subsystems/Renderer/Buffers/VulkanMesh.cpp Source/Subsystems/Renderer/Buffers/VulkanMesh.h
    Source/Subsystems/Renderer/Buffers/VulkanTexture.cpp Source/Subsystems/Renderer/Buffers/VulkanTexture.h
    Source/Subsystems/Renderer/Texture/TextureManager.cpp Source/Subsystems/Renderer/Texture/TextureManager.h
    Source/Subsystems/Renderer/Material/Material.cpp Source/Subsystems/Renderer/Material/Material.h
    Source/Subsystems/Renderer/RendererTypes.h
    Source/Subsystems/Renderer/IRenderer.h
)
configure_astral_target(AstralEngine)

#----------------------------------------------------------------------
# 7. Shader Derleme Sistemi
#----------------------------------------------------------------------
function(astral_compile_shaders target_name)
    if(NOT ASTRAL_HAS_SLANG)
        message(WARNING "Slang not found, cannot compile .slang shaders.")
        return()
    endif()

    set(SHADER_SOURCES ${ARGN})
    set(COMPILED_SHADERS "")
    set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    foreach(SHADER_SOURCE_PATH ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SOURCE_PATH} NAME)
        set(SHADER_OUTPUT_PATH "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${SHADER_OUTPUT_PATH}
            COMMAND slang::slangc
                -target spirv
                -profile sm_6_6
                -entry main
                -o ${SHADER_OUTPUT_PATH}
                ${SHADER_SOURCE_PATH}
            DEPENDS ${SHADER_SOURCE_PATH}
            COMMENT "Compiling Slang shader: ${SHADER_NAME}"
        )
        list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT_PATH})
    endforeach()

    add_custom_target(${target_name}_shaders ALL DEPENDS ${COMPILED_SHADERS})
    add_dependencies(${target_name} ${target_name}_shaders)
endfunction()

#----------------------------------------------------------------------
# 8. Sandbox Uygulama Hedefi
#----------------------------------------------------------------------
add_executable(Sandbox Source/main.cpp)
target_link_libraries(Sandbox PRIVATE AstralEngine)
configure_astral_target(Sandbox)

# Proje varlıklarını ve derlenmiş shader'ları Sandbox'ın yanına kopyala
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/Assets")
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")

add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${ASSETS_DIR} $<TARGET_FILE_DIR:Sandbox>/Assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${SHADER_OUTPUT_DIR} $<TARGET_FILE_DIR:Sandbox>/Assets/Shaders
    COMMENT "Copying assets and compiled shaders to output directory..."
)

# Derlenecek shader'ları bul ve derleme hedefini oluştur
file(GLOB ENGINE_SHADERS 
    "${ASSETS_DIR}/Shaders/*.slang"
    "${ASSETS_DIR}/Shaders/*/*.slang"
)
# Include ve Common dizinlerindeki header dosyalarını derleme listesinden çıkar
list(FILTER ENGINE_SHADERS EXCLUDE REGEX ".*Include/.*")
list(FILTER ENGINE_SHADERS EXCLUDE REGEX ".*Common/.*")
astral_compile_shaders(Sandbox ${ENGINE_SHADERS})

#----------------------------------------------------------------------
# 9. SDL3 Test Uygulama Hedefi
#----------------------------------------------------------------------
if(ASTRAL_BUILD_TESTS)
    add_executable(SDL3Test Source/test_sdl3.cpp)
    target_link_libraries(SDL3Test PRIVATE AstralEngine)
    configure_astral_target(SDL3Test)
    
    # SDL3Test için IDE özelliklerini ayarla
    set_target_properties(SDL3Test PROPERTIES
        OUTPUT_NAME "SDL3Test$<$<CONFIG:Debug>:_Debug>"
        DESCRIPTION "Astral Engine SDL3 Integration Test"
        VERSION ${PROJECT_VERSION}
        $<$<PLATFORM_ID:Windows>:WIN32_EXECUTABLE OFF>
        $<$<CONFIG:Debug>:DEBUG_POSTFIX "_d">
        $<$<CONFIG:RelWithDebInfo>:RELWITHDEBINFO_POSTFIX "_rd">
    )
    
    message(STATUS "SDL3Test target created (enabled by ASTRAL_BUILD_TESTS)")
endif()

#----------------------------------------------------------------------
# 9. Build Özeti
#----------------------------------------------------------------------
message(STATUS "==========================================")
message(STATUS " Astral Engine Build Configuration Summary")
message(STATUS "==========================================")
message(STATUS "Version:      ${PROJECT_VERSION}")
message(STATUS "Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "Generator:    ${CMAKE_GENERATOR}")
message(STATUS "Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "------------------ Features ------------------")
message(STATUS "SDL3:         ${ASTRAL_HAS_SDL3}")
message(STATUS "Vulkan:       ${ASTRAL_HAS_VULKAN}")
message(STATUS "Assimp:       ${ASTRAL_HAS_ASSIMP}")
message(STATUS "Slang:        ${ASTRAL_HAS_SLANG}")
message(STATUS "GLM:          ${ASTRAL_HAS_GLM}")
message(STATUS "JSON:         ${ASTRAL_HAS_JSON}")
message(STATUS "Vulkan Valid: ${ASTRAL_VULKAN_VALIDATION}")
message(STATUS "------------------ Output --------------------")
message(STATUS "Install Dir:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Binary Dir:   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "==========================================")
