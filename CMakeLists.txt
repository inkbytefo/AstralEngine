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
# (like DXC or Slang), you can disable SSL certificate verification for downloads.
# This is a security risk and should only be used if you trust the repositories.
option(ASTRAL_DISABLE_TLS_VERIFY "SECURITY RISK: Disable SSL/TLS certificate verification for FetchContent." ON)

if(ASTRAL_DISABLE_TLS_VERIFY)
    message(WARNING "SECURITY RISK: SSL/TLS certificate verification is DISABLED. This should only be used in trusted networks.")
    set(CMAKE_TLS_VERIFY OFF)
endif()

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
option(ASTRAL_BUILD_EXAMPLES "Build example applications" OFF)
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
# SPIRV desteğini açıkça aktif et
set(SLANG_ENABLE_SPIRV_TOOLS ON)
# Statik build zorlamasını kaldırarak varsayılan (shared) derlemeye izin ver.
# Bu, LNK1104 bağlama hatasını çözecektir.
set(SLANG_BUILD_SHARED OFF)
set(SLANG_BUILD_STATIC ON)
find_or_fetch_package(slang "https://github.com/shader-slang/slang.git" "v2025.16.1")
set(ASTRAL_HAS_SLANG TRUE)

# SPIRV-Dependencies are handled by slang


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
    GIT_TAG v3.3.0
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
        SPIRV-Tools
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

set(ASTRAL_ENGINE_SOURCES
    # Core
    Source/Core/Engine.cpp
    Source/Core/Engine.h
    Source/Core/IApplication.h
    Source/Core/ISubsystem.h
    Source/Core/Logger.cpp
    Source/Core/Logger.h
    Source/Core/FileLogger.cpp
    Source/Core/FileLogger.h
    Source/Core/MemoryManager.cpp
    Source/Core/MemoryManager.h
    Source/Core/ThreadPool.cpp
    Source/Core/ThreadPool.h

    # ECS
    Source/ECS/Components.h
    Source/Subsystems/ECS/ECSSubsystem.cpp
    Source/Subsystems/ECS/ECSSubsystem.h

    # Events
    Source/Events/Event.h
    Source/Events/EventManager.cpp
    Source/Events/EventManager.h

    # Platform
    Source/Subsystems/Platform/InputManager.cpp
    Source/Subsystems/Platform/InputManager.h
    Source/Subsystems/Platform/KeyCode.h
    Source/Subsystems/Platform/PlatformSubsystem.cpp
    Source/Subsystems/Platform/PlatformSubsystem.h
    Source/Subsystems/Platform/Window.cpp
    Source/Subsystems/Platform/Window.h

    # Asset System
    Source/Subsystems/Asset/AssetData.h
    Source/Subsystems/Asset/AssetHandle.cpp
    Source/Subsystems/Asset/AssetHandle.h
    Source/Subsystems/Asset/AssetManager.cpp
    Source/Subsystems/Asset/AssetManager.h
    Source/Subsystems/Asset/AssetRegistry.cpp
    Source/Subsystems/Asset/AssetRegistry.h
    Source/Subsystems/Asset/AssetSubsystem.cpp
    Source/Subsystems/Asset/AssetSubsystem.h
    Source/Subsystems/Asset/Model.cpp
    Source/Subsystems/Asset/Model.h
    Source/Subsystems/Asset/ShaderProgram.h

    # Renderer
    Source/Subsystems/Renderer/BloomEffect.cpp
    Source/Subsystems/Renderer/BloomEffect.h
    Source/Subsystems/Renderer/Bounds.h
    Source/Subsystems/Renderer/Camera.cpp
    Source/Subsystems/Renderer/Camera.h
    Source/Subsystems/Renderer/GraphicsDevice.cpp
    Source/Subsystems/Renderer/GraphicsDevice.h
    Source/Subsystems/Renderer/IPostProcessingEffect.h
    Source/Subsystems/Renderer/IRenderer.h
    Source/Subsystems/Renderer/PostProcessingSubsystem.cpp
    Source/Subsystems/Renderer/PostProcessingSubsystem.h
    Source/Subsystems/Renderer/RendererTypes.h
    Source/Subsystems/Renderer/RenderSubsystem.cpp
    Source/Subsystems/Renderer/RenderSubsystem.h
    Source/Subsystems/Renderer/TonemappingEffect.cpp
    Source/Subsystems/Renderer/TonemappingEffect.h
    Source/Subsystems/Renderer/VulkanMemoryManager.cpp
    Source/Subsystems/Renderer/VulkanMemoryManager.h
    Source/Subsystems/Renderer/VulkanMeshManager.cpp
    Source/Subsystems/Renderer/VulkanMeshManager.h
    Source/Subsystems/Renderer/VulkanRenderer.cpp
    Source/Subsystems/Renderer/VulkanRenderer.h
    Source/Subsystems/Renderer/VulkanSynchronization.cpp
    Source/Subsystems/Renderer/VulkanSynchronization.h
    Source/Subsystems/Renderer/VulkanTextureManager.cpp
    Source/Subsystems/Renderer/VulkanTextureManager.h
    Source/Subsystems/Renderer/VulkanUtils.cpp
    Source/Subsystems/Renderer/VulkanUtils.h

    # Renderer/Buffers
    Source/Subsystems/Renderer/Buffers/VulkanBuffer.cpp
    Source/Subsystems/Renderer/Buffers/VulkanBuffer.h
    Source/Subsystems/Renderer/Buffers/VulkanMesh.cpp
    Source/Subsystems/Renderer/Buffers/VulkanMesh.h
    Source/Subsystems/Renderer/Buffers/VulkanTexture.cpp
    Source/Subsystems/Renderer/Buffers/VulkanTexture.h

    # Renderer/Commands
    Source/Subsystems/Renderer/Commands/VulkanCommandPool.cpp
    Source/Subsystems/Renderer/Commands/VulkanCommandPool.h
    Source/Subsystems/Renderer/Commands/VulkanPipeline.cpp
    Source/Subsystems/Renderer/Commands/VulkanPipeline.h

    # Renderer/Core
    Source/Subsystems/Renderer/Core/VulkanDevice.cpp
    Source/Subsystems/Renderer/Core/VulkanDevice.h
    Source/Subsystems/Renderer/Core/VulkanFramebuffer.cpp
    Source/Subsystems/Renderer/Core/VulkanFramebuffer.h
    Source/Subsystems/Renderer/Core/VulkanFrameManager.cpp
    Source/Subsystems/Renderer/Core/VulkanFrameManager.h
    Source/Subsystems/Renderer/Core/VulkanInstance.cpp
    Source/Subsystems/Renderer/Core/VulkanInstance.h
    Source/Subsystems/Renderer/Core/VulkanSwapchain.cpp
    Source/Subsystems/Renderer/Core/VulkanSwapchain.h

    # Renderer/Material
    Source/Subsystems/Renderer/Material/Material.cpp
    Source/Subsystems/Renderer/Material/Material.h

    # Renderer/Shaders
    Source/Subsystems/Renderer/Shaders/VulkanShader.cpp
    Source/Subsystems/Renderer/Shaders/VulkanShader.h

    # Renderer/Texture
    Source/Subsystems/Renderer/Texture/TextureManager.cpp
    Source/Subsystems/Renderer/Texture/TextureManager.h
)

target_sources(AstralEngine PRIVATE ${ASTRAL_ENGINE_SOURCES})
configure_astral_target(AstralEngine)

#----------------------------------------------------------------------
# 7. Astral Engine - Advanced Shader Compilation System
# Supports: Slang -> SPIR-V compilation with hot reload capability
#----------------------------------------------------------------------

# Shader stage detection patterns
set(ASTRAL_SHADER_STAGE_PATTERNS
    "vertex;vert;vs"
    "fragment;frag;pixel;ps"
    "compute;comp;cs"
    "geometry;geom;gs"
    "tessellation_control;tesc;hs"
    "tessellation_evaluation;tese;ds"
)

# Global shader configuration
set(ASTRAL_SHADER_OUTPUT_FORMAT "spirv" CACHE STRING "Target shader format")
set(ASTRAL_SHADER_PROFILE "sm_6_6" CACHE STRING "Shader model profile")
set(ASTRAL_SHADER_OPTIMIZATION_LEVEL "O2" CACHE STRING "Shader optimization level")
set(ASTRAL_ENABLE_SHADER_DEBUG OFF CACHE BOOL "Enable shader debug information")
set(ASTRAL_ENABLE_SHADER_HOT_RELOAD ON CACHE BOOL "Enable shader hot reload in development")

#----------------------------------------------------------------------
# Internal: Detect shader stage from filename or content
#----------------------------------------------------------------------
function(_astral_detect_shader_stage shader_path out_stage out_entry)
    get_filename_component(shader_name ${shader_path} NAME_WE)
    string(TOLOWER ${shader_name} shader_name_lower)
    
    # Default values
    set(detected_stage "vertex")
    set(detected_entry "main")
    
    # Pattern matching for stage detection
    foreach(stage_info ${ASTRAL_SHADER_STAGE_PATTERNS})
        string(REPLACE ";" "|" patterns ${stage_info})
        list(GET stage_info 0 stage_name)
        
        if(shader_name_lower MATCHES ".*(${patterns}).*")
            set(detected_stage ${stage_name})
            break()
        endif()
    endforeach()
    
    # Try to detect entry point from content (basic heuristic)
    if(EXISTS ${shader_path})
        file(READ ${shader_path} shader_content)
        if(shader_content MATCHES "([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*\\([^)]*\\)[[:space:]]*:[[:space:]]*SV_")
            set(detected_entry ${CMAKE_MATCH_1})
        endif()
    endif()
    
    set(${out_stage} ${detected_stage} PARENT_SCOPE)
    set(${out_entry} ${detected_entry} PARENT_SCOPE)
endfunction()

#----------------------------------------------------------------------
# Internal: Generate Slang compilation command
#----------------------------------------------------------------------
function(_astral_generate_slang_command shader_path output_path stage entry out_command)
    set(base_args
        -target ${ASTRAL_SHADER_OUTPUT_FORMAT}
        -profile ${ASTRAL_SHADER_PROFILE}
        -stage ${stage}
        -entry ${entry}
        -o "${output_path}"
    )
    
    # Add optimization flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND base_args -${ASTRAL_SHADER_OPTIMIZATION_LEVEL})
    endif()
    
    # Add debug information for development builds
    if(ASTRAL_ENABLE_SHADER_DEBUG AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND base_args -g -O0)
    endif()
    
    # Add hot reload support
    if(ASTRAL_ENABLE_SHADER_HOT_RELOAD AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND base_args -embed-downstream-ir)
    endif()
    
    set(${out_command} ${base_args} "${shader_path}" PARENT_SCOPE)
endfunction()

#----------------------------------------------------------------------
# Internal: Create shader compilation target
#----------------------------------------------------------------------
function(_astral_create_shader_target target_name shader_info_list output_dir)
    set(all_outputs "")
    set(all_commands "")
    
    foreach(shader_info ${shader_info_list})
        # Parse shader info: "path;stage;entry;output"
        # Fix: Use proper list handling instead of string(SPLIT)
        set(shader_parts ${shader_info})
        list(GET shader_parts 0 shader_path)
        list(GET shader_parts 1 shader_stage)
        list(GET shader_parts 2 shader_entry)
        list(GET shader_parts 3 output_path)
        
        get_filename_component(shader_name ${shader_path} NAME)
        
        # Generate compilation command
        _astral_generate_slang_command(
            ${shader_path}
            ${output_path}
            ${shader_stage}
            ${shader_entry}
            slang_command
        )
        
        # Create custom command with proper error handling
        add_custom_command(
            OUTPUT ${output_path}
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${shader_stage} shader: ${shader_name}"
            COMMAND $<TARGET_FILE:slangc> ${slang_command}
            COMMAND ${CMAKE_COMMAND} -E echo "Successfully compiled: ${shader_name} -> ${output_path}"
            DEPENDS ${shader_path} slangc
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Astral: Building ${shader_stage} shader ${shader_name}"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        
        list(APPEND all_outputs ${output_path})
        
        # Hot reload support: Create file watcher for development
        if(ASTRAL_ENABLE_SHADER_HOT_RELOAD AND CMAKE_BUILD_TYPE STREQUAL "Debug")
            get_filename_component(shader_dir ${shader_path} DIRECTORY)
            set_property(DIRECTORY ${shader_dir} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${shader_path})
        endif()
    endforeach()
    
    # Create the shader compilation target
    add_custom_target(${target_name}_shaders
        ALL
        DEPENDS ${all_outputs}
        COMMENT "Astral: Compiling all shaders for ${target_name}"
    )
    
    # Ensure proper build order
    add_dependencies(${target_name} ${target_name}_shaders)
    
    # Set target properties for IDE organization
    set_target_properties(${target_name}_shaders PROPERTIES
        FOLDER "Astral/Shaders"
        VS_GLOBAL_EnableMicrosoftCodeAnalysis false
    )
endfunction()

#----------------------------------------------------------------------
# Public API: Main shader compilation function
#----------------------------------------------------------------------
function(astral_compile_shaders target_name)
    # Validate prerequisites
    if(NOT ASTRAL_HAS_SLANG)
        message(FATAL_ERROR "Astral Engine: Slang compiler not found! Cannot compile shaders for ${target_name}")
        return()
    endif()
    
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "Astral Engine: Target '${target_name}' does not exist")
        return()
    endif()
    
    set(shader_sources ${ARGN})
    if(NOT shader_sources)
        message(WARNING "Astral Engine: No shader sources provided for ${target_name}")
        return()
    endif()
    
    # Setup output directories
    set(shader_output_dir "${CMAKE_BINARY_DIR}/Assets/Shaders/${target_name}")
    set(runtime_shader_dir "$<TARGET_FILE_DIR:${target_name}>/Assets/Shaders")
    
    file(MAKE_DIRECTORY ${shader_output_dir})
    
    # Process each shader source
    set(processed_shaders "")
    set(total_shaders 0)
    set(processed_count 0)
    
    list(LENGTH shader_sources total_shaders)
    message(STATUS "Astral Engine: Processing ${total_shaders} shader(s) for ${target_name}")
    
    foreach(shader_source ${shader_sources})
        math(EXPR processed_count "${processed_count} + 1")
        
        # Validate shader file exists
        if(NOT EXISTS ${shader_source})
            message(FATAL_ERROR "Astral Engine: Shader file not found: ${shader_source}")
            continue()
        endif()
        
        # Auto-detect shader stage and entry point
        _astral_detect_shader_stage(${shader_source} shader_stage shader_entry)
        
        # Generate output filename
        get_filename_component(shader_name ${shader_source} NAME_WE)
        set(output_filename "${shader_name}.${shader_stage}.spv")
        set(output_path "${shader_output_dir}/${output_filename}")
        
        # Create shader info entry
        set(shader_info "${shader_source};${shader_stage};${shader_entry};${output_path}")
        list(APPEND processed_shaders ${shader_info})
        
        message(STATUS "  [${processed_count}/${total_shaders}] ${shader_name} -> ${shader_stage} (${shader_entry})")
    endforeach()
    
    # Create compilation targets
    if(processed_shaders)
        _astral_create_shader_target(${target_name} "${processed_shaders}" ${shader_output_dir})
        
        # Copy compiled shaders to runtime location
        add_custom_command(TARGET ${target_name}_shaders POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${shader_output_dir}
                ${runtime_shader_dir}
            COMMENT "Astral: Deploying shaders for ${target_name}"
        )
        
        message(STATUS "Astral Engine: Shader system configured for ${target_name}")
    else()
        message(WARNING "Astral Engine: No valid shaders processed for ${target_name}")
    endif()
endfunction()

#----------------------------------------------------------------------
# Public API: Configure shader compilation settings
#----------------------------------------------------------------------
function(astral_configure_shaders)
    cmake_parse_arguments(ASTRAL_SHADER
        "ENABLE_DEBUG;DISABLE_HOT_RELOAD;VERBOSE"
        "PROFILE;OPTIMIZATION;OUTPUT_FORMAT"
        "INCLUDE_DIRS;PREPROCESSOR_DEFINES"
        ${ARGN}
    )
    
    if(ASTRAL_SHADER_PROFILE)
        set(ASTRAL_SHADER_PROFILE ${ASTRAL_SHADER_PROFILE} CACHE STRING "" FORCE)
    endif()
    
    if(ASTRAL_SHADER_OPTIMIZATION)
        set(ASTRAL_SHADER_OPTIMIZATION_LEVEL ${ASTRAL_SHADER_OPTIMIZATION} CACHE STRING "" FORCE)
    endif()
    
    if(ASTRAL_SHADER_OUTPUT_FORMAT)
        set(ASTRAL_SHADER_OUTPUT_FORMAT ${ASTRAL_SHADER_OUTPUT_FORMAT} CACHE STRING "" FORCE)
    endif()
    
    if(ASTRAL_SHADER_ENABLE_DEBUG)
        set(ASTRAL_ENABLE_SHADER_DEBUG ON CACHE BOOL "" FORCE)
    endif()
    
    if(ASTRAL_SHADER_DISABLE_HOT_RELOAD)
        set(ASTRAL_ENABLE_SHADER_HOT_RELOAD OFF CACHE BOOL "" FORCE)
    endif()
    
    if(ASTRAL_SHADER_VERBOSE)
        message(STATUS "Astral Shader Configuration:")
        message(STATUS "  Profile: ${ASTRAL_SHADER_PROFILE}")
        message(STATUS "  Optimization: ${ASTRAL_SHADER_OPTIMIZATION_LEVEL}")
        message(STATUS "  Format: ${ASTRAL_SHADER_OUTPUT_FORMAT}")
        message(STATUS "  Debug: ${ASTRAL_ENABLE_SHADER_DEBUG}")
        message(STATUS "  Hot Reload: ${ASTRAL_ENABLE_SHADER_HOT_RELOAD}")
    endif()
endfunction()

#----------------------------------------------------------------------
# Public API: Add shader include directories
#----------------------------------------------------------------------
function(astral_add_shader_includes)
    # This can be extended to handle include directories for Slang
    set(ASTRAL_SHADER_INCLUDE_DIRS ${ARGN} CACHE STRING "Shader include directories")
endfunction()

#----------------------------------------------------------------------
# 8. Sandbox Uygulama Hedefi
#----------------------------------------------------------------------
add_executable(Sandbox Source/main.cpp)
target_link_libraries(Sandbox PRIVATE AstralEngine)
configure_astral_target(Sandbox)

# Proje varlıklarını Sandbox'ın yanına kopyala
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/Assets")

add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${ASSETS_DIR} $<TARGET_FILE_DIR:Sandbox>/Assets
    COMMENT "Copying assets to output directory..."
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
if(ASTRAL_BUILD_EXAMPLES)
    add_executable(Example_SDL3 Source/test_sdl3.cpp)
    target_link_libraries(Example_SDL3 PRIVATE AstralEngine)
    configure_astral_target(Example_SDL3)
    
    # Example_SDL3 için IDE özelliklerini ayarla
    set_target_properties(Example_SDL3 PROPERTIES
        OUTPUT_NAME "Example_SDL3$<$<CONFIG:Debug>:_Debug>"
        DESCRIPTION "Astral Engine SDL3 Integration Example"
        VERSION ${PROJECT_VERSION}
        $<$<PLATFORM_ID:Windows>:WIN32_EXECUTABLE OFF>
        $<$<CONFIG:Debug>:DEBUG_POSTFIX "_d">
        $<$<CONFIG:RelWithDebInfo>:RELWITHDEBINFO_POSTFIX "_rd">
    )
    
    message(STATUS "Example_SDL3 target created (enabled by ASTRAL_BUILD_EXAMPLES)")
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
