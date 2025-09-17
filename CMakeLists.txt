# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Advanced 3D Game Engine
#  *
#  * CMake Configuration File
#  * Purpose: Modern, modular CMake configuration for AstralEngine
#  * Author: Astral Engine Development Team
#  * Version: 0.1.0-alpha
#  * Date: 2025-09-17
#  * Signature: inkbytefo
#  **********************************************************************/

cmake_minimum_required(VERSION 3.24)
project(AstralEngine VERSION 0.1.0 LANGUAGES CXX)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Modern CMake practices
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type validation
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# External dependencies directory
set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/External)

# Enable FetchContent for dependency management
include(FetchContent)

# Disable SSL certificate verification to fix download issues
set(CMAKE_TLS_VERIFY 0 CACHE BOOL "Disable SSL certificate verification" FORCE)

# /**********************************************************************
#  * Build Options
#  **********************************************************************/
# Core options
option(ASTRAL_BUILD_EXAMPLES "Build example applications" ON)
option(ASTRAL_BUILD_TESTS "Build unit tests" OFF)
option(ASTRAL_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(ASTRAL_ENABLE_PROFILING "Enable profiling support" OFF)

# Vendor options
option(ASTRAL_VENDOR_SDL3 "Fetch and build SDL3 from source if not found" OFF)
option(ASTRAL_VENDOR_VULKAN "Fetch and build Vulkan dependencies from source if not found" OFF)
option(ASTRAL_VENDOR_ASSIMP "Fetch and build Assimp from source if not found" ON)

# Feature options
option(ASTRAL_ENABLE_VULKAN "Enable Vulkan graphics API support" ON)
option(ASTRAL_VULKAN_VALIDATION "Enable Vulkan validation layers in debug builds" ON)
option(ASTRAL_LINK_SDL3MAIN "Link SDL3main on Windows for WinMain wrapping" ON)

# Version configurations
set(ASTRAL_SDL3_VERSION "3.2.22" CACHE STRING "Target SDL3 version")
set(ASTRAL_ASSIMP_VERSION "6.0.2" CACHE STRING "Target Assimp version")
set(ASTRAL_VULKAN_VERSION "1.4.0" CACHE STRING "Target Vulkan SDK version")

# /**********************************************************************
#  * Compiler Configuration
#  **********************************************************************/
if(MSVC)
    # MSVC specific options
    add_compile_options(
        /W4                    # High warning level
        /permissive-           # Standards conformance
        /Zc:preprocessor       # Standards-conforming preprocessor
        /Zc:__cplusplus        # Enable __cplusplus macro update
        /EHsc                  # Exception handling model
        /source-charset:utf-8  # Use UTF-8 for source character set
        /execution-charset:utf-8 # Use UTF-8 for execution character set
        /MP                    # Multi-processor compilation
        /bigobj                # Large object files support
    )
    
    # Optional warnings as errors
    if(ASTRAL_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
    
    # Debug configuration
    add_compile_options($<$<CONFIG:Debug>:/Od>)
    add_compile_options($<$<CONFIG:Debug>:/ZI>)
    
    # Release configuration
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/Ob3>)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    
    # Linker options
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    
else()
    # GCC/Clang options
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual
        -Wpedantic -Wconversion
        -Wsign-conversion -Wmisleading-indentation
    )
    
    if(ASTRAL_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
    
    # Debug configuration
    add_compile_options($<$<CONFIG:Debug>:-O0>)
    add_compile_options($<$<CONFIG:Debug>:-g3>)
    
    # Release configuration
    add_compile_options($<$<CONFIG:Release>:-O3>)
    add_compile_options($<$<CONFIG:Release>:-DNDEBUG>)
    
    # Enable position independent code
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# /**********************************************************************
#  * Dependency Management
#  **********************************************************************/

# SDL3 Configuration
set(SDL3_FOUND FALSE)
set(ASTRAL_HAS_SDL3 FALSE)

# Strategy 1: Check for pre-built SDL3 in External directory
if(EXISTS "${EXTERNAL_DIR}/SDL3/include/SDL3" AND EXISTS "${EXTERNAL_DIR}/SDL3/lib")
    message(STATUS "Found pre-built SDL3 in External/SDL3 directory")
    set(SDL3_FOUND TRUE)
    set(ASTRAL_USE_PREBUILT_SDL3 TRUE)
elseif(EXISTS "${EXTERNAL_DIR}/SDL3/CMakeLists.txt")
    message(STATUS "Found source SDL3 in External/SDL3 directory")
    set(SDL3_FOUND TRUE)
    set(ASTRAL_USE_LOCAL_SDL3 TRUE)
else()
    # Strategy 2: Try to find SDL3 via Config mode
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        message(STATUS "SDL3 found via system installation")
    endif()
endif()

# Strategy 3: FetchContent from source if enabled
if(NOT SDL3_FOUND AND ASTRAL_VENDOR_SDL3)
    message(STATUS "Fetching SDL3 from source (version: ${ASTRAL_SDL3_VERSION})...")
    
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-${ASTRAL_SDL3_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    
    # Configure SDL3 build options
    set(SDL_SHARED ON CACHE BOOL "Build SDL3 as shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL3 as static library" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL3 test library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
    set(SDL_INSTALL_TESTS OFF CACHE BOOL "Install SDL3 tests" FORCE)
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "Disable SDL3 install" FORCE)
    set(SDL_ASSERTIONS disabled CACHE STRING "SDL assertion level" FORCE)
    
    FetchContent_MakeAvailable(SDL3)
    set(SDL3_FOUND TRUE)
    message(STATUS "SDL3 successfully built from source")
endif()

# Handle pre-built SDL3
if(ASTRAL_USE_PREBUILT_SDL3)
    # Determine architecture-specific library path
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(SDL3_ARCH_DIR "x64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
        set(SDL3_ARCH_DIR "x86")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(SDL3_ARCH_DIR "arm64")
    else()
        set(SDL3_ARCH_DIR "x64")
    endif()
    
    set(SDL3_LIB_DIR "${EXTERNAL_DIR}/SDL3/lib/${SDL3_ARCH_DIR}")
    
    if(NOT EXISTS "${SDL3_LIB_DIR}/SDL3.lib")
        message(FATAL_ERROR "SDL3.lib not found at ${SDL3_LIB_DIR}")
    endif()
    
    add_library(SDL3::SDL3 SHARED IMPORTED)
    set_target_properties(SDL3::SDL3 PROPERTIES
        IMPORTED_LOCATION "${SDL3_LIB_DIR}/SDL3.dll"
        IMPORTED_IMPLIB "${SDL3_LIB_DIR}/SDL3.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${EXTERNAL_DIR}/SDL3/include"
    )
    
    set(SDL3_DLL_PATH "${SDL3_LIB_DIR}/SDL3.dll")
    message(STATUS "Pre-built SDL3 configured successfully (${SDL3_ARCH_DIR})")
endif()

# Handle local SDL3 build
if(ASTRAL_USE_LOCAL_SDL3)
    set(SDL_SHARED ON CACHE BOOL "Build SDL3 as shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL3 as static library" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL3 test library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
    set(SDL_INSTALL_TESTS OFF CACHE BOOL "Install SDL3 tests" FORCE)
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "Disable SDL3 install" FORCE)
    set(SDL_ASSERTIONS disabled CACHE STRING "SDL assertion level" FORCE)
    
    add_subdirectory(${EXTERNAL_DIR}/SDL3 EXCLUDE_FROM_ALL)
    message(STATUS "Local SDL3 build configured successfully")
endif()

if(SDL3_FOUND)
    set(ASTRAL_HAS_SDL3 TRUE)
    message(STATUS "SDL3 integration completed successfully")
else()
    message(FATAL_ERROR 
        "SDL3 not found. Please either:\n"
        "  1. Install SDL3 development libraries\n"
        "  2. Set SDL3_ROOT to your SDL3 installation path\n"
        "  3. Enable source build: -DASTRAL_VENDOR_SDL3=ON\n"
        "  4. Download SDL3 to ${EXTERNAL_DIR}/SDL3"
    )
endif()

# Vulkan Configuration
set(VULKAN_FOUND FALSE)
set(ASTRAL_HAS_VULKAN FALSE)

if(ASTRAL_ENABLE_VULKAN)
    message(STATUS "Configuring Vulkan ${ASTRAL_VULKAN_VERSION} support...")
    
    # Strategy 1: Check for pre-built Vulkan SDK in External directory
    if(EXISTS "${EXTERNAL_DIR}/Vulkan/include/vulkan" AND EXISTS "${EXTERNAL_DIR}/Vulkan/lib")
        message(STATUS "Found pre-built Vulkan SDK in External/Vulkan directory")
        set(VULKAN_FOUND TRUE)
        set(ASTRAL_USE_PREBUILT_VULKAN TRUE)
        
        # Determine architecture-specific library path
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(VULKAN_ARCH_DIR "x64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
            set(VULKAN_ARCH_DIR "x86")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(VULKAN_ARCH_DIR "arm64")
        else()
            set(VULKAN_ARCH_DIR "x64")
        endif()
        
        set(VULKAN_LIB_DIR "${EXTERNAL_DIR}/Vulkan/lib/${VULKAN_ARCH_DIR}")
        
        add_library(Vulkan::Vulkan INTERFACE IMPORTED)
        target_include_directories(Vulkan::Vulkan INTERFACE "${EXTERNAL_DIR}/Vulkan/include")
        target_link_directories(Vulkan::Vulkan INTERFACE "${VULKAN_LIB_DIR}")
        
        if(WIN32)
            target_link_libraries(Vulkan::Vulkan INTERFACE vulkan-1)
        else()
            target_link_libraries(Vulkan::Vulkan INTERFACE vulkan)
        endif()
        
        message(STATUS "Pre-built Vulkan SDK configured successfully (${VULKAN_ARCH_DIR})")
    endif()
    
    # Strategy 2: Try to find Vulkan SDK via system installation
    if(NOT VULKAN_FOUND)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            message(STATUS "Vulkan SDK found via system installation")
        endif()
    endif()
    
    # Strategy 3: Try to find Vulkan via environment variable
    if(NOT VULKAN_FOUND AND DEFINED ENV{VULKAN_SDK})
        set(VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}")
        if(EXISTS "${VULKAN_SDK_ROOT}/Include/vulkan" AND EXISTS "${VULKAN_SDK_ROOT}/Lib")
            set(VULKAN_FOUND TRUE)
            set(ASTRAL_USE_SYSTEM_VULKAN TRUE)
            
            add_library(Vulkan::Vulkan INTERFACE IMPORTED)
            target_include_directories(Vulkan::Vulkan INTERFACE "${VULKAN_SDK_ROOT}/Include")
            target_link_directories(Vulkan::Vulkan INTERFACE "${VULKAN_SDK_ROOT}/Lib")
            
            if(WIN32)
                target_link_libraries(Vulkan::Vulkan INTERFACE vulkan-1)
            else()
                target_link_libraries(Vulkan::Vulkan INTERFACE vulkan)
            endif()
            
            message(STATUS "Vulkan SDK found via VULKAN_SDK environment variable")
        endif()
    endif()
    
    # Strategy 4: FetchContent for Vulkan dependencies if enabled
    if(NOT VULKAN_FOUND AND ASTRAL_VENDOR_VULKAN)
        message(STATUS "Fetching Vulkan dependencies from source...")
        
        FetchContent_Declare(
            VulkanHeaders
            GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
            GIT_TAG v${ASTRAL_VULKAN_VERSION}
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        
        FetchContent_Declare(
            VulkanMemoryAllocator
            GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
            GIT_TAG v3.1.0
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        
        FetchContent_MakeAvailable(VulkanHeaders VulkanMemoryAllocator)
        set(VULKAN_FOUND TRUE)
        message(STATUS "Vulkan dependencies fetched successfully")
    endif()
    
    if(VULKAN_FOUND)
        set(ASTRAL_HAS_VULKAN TRUE)
        message(STATUS "Vulkan ${ASTRAL_VULKAN_VERSION} support configured successfully")
    else()
        message(WARNING 
            "Vulkan SDK not found. Vulkan support will be disabled.\n"
            "Please either:\n"
            "  1. Install Vulkan SDK\n"
            "  2. Set VULKAN_SDK environment variable\n"
            "  3. Download Vulkan SDK to ${EXTERNAL_DIR}/Vulkan\n"
            "  4. Enable source build: -DASTRAL_VENDOR_VULKAN=ON"
        )
        set(ASTRAL_ENABLE_VULKAN FALSE)
    endif()
endif()

# Assimp Configuration
set(ASSIMP_FOUND FALSE)
set(ASTRAL_HAS_ASSIMP FALSE)

# Strategy: FetchContent for Assimp if enabled
if(ASTRAL_VENDOR_ASSIMP)
    message(STATUS "Fetching Assimp from source (version: ${ASTRAL_ASSIMP_VERSION})...")
    
    FetchContent_Declare(
        Assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v${ASTRAL_ASSIMP_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    
    # Configure Assimp build options
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Build Assimp tests" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Build Assimp tools" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "Install Assimp" FORCE)
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Build zlib" FORCE)
    
    FetchContent_MakeAvailable(Assimp)
    set(ASSIMP_FOUND TRUE)
    message(STATUS "Assimp successfully built from source")
endif()

if(ASSIMP_FOUND)
    set(ASTRAL_HAS_ASSIMP TRUE)
    message(STATUS "Assimp integration completed successfully")
else()
    message(WARNING 
        "Assimp not found. Model loading support will be disabled.\n"
        "Please either:\n"
        "  1. Install Assimp development libraries\n"
        "  2. Download Assimp to ${EXTERNAL_DIR}/Assimp\n"
        "  3. Enable source build: -DASTRAL_VENDOR_ASSIMP=ON"
    )
endif()

# **********************************************************************
# * Slang Configuration
# **********************************************************************
set(SLANG_FOUND FALSE)
set(ASTRAL_HAS_SLANG FALSE)

option(ASTRAL_VENDOR_SLANG "Fetch and build Slang from source if not found" ON)

# Strategy 1: Check for pre-built Slang in External directory
if(EXISTS "${EXTERNAL_DIR}/slang/include/slang" AND EXISTS "${EXTERNAL_DIR}/slang/lib")
    message(STATUS "Found pre-built Slang in External/slang directory")
    set(SLANG_FOUND TRUE)
    set(ASTRAL_USE_PREBUILT_SLANG TRUE)
else()
    # Strategy 2: Try to find Slang via Config mode
    find_package(slang CONFIG QUIET)
    if(slang_FOUND)
        message(STATUS "Slang found via system installation")
        set(SLANG_FOUND TRUE)
    endif()
endif()

# Strategy 3: FetchContent from source if enabled
if(NOT SLANG_FOUND AND ASTRAL_VENDOR_SLANG)
    message(STATUS "Fetching Slang from source...")
    
    FetchContent_Declare(
        slang
        GIT_REPOSITORY https://github.com/shader-slang/slang.git
        GIT_TAG v2025.16.1  # Use latest stable version
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    
    # Configure Slang build options
    set(SLANG_ENABLE_SLANGC ON CACHE BOOL "Enable standalone compiler target" FORCE)
    set(SLANG_ENABLE_TESTS OFF CACHE BOOL "Enable test targets" FORCE)
    set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "Enable example targets" FORCE)
    set(SLANG_LIB_TYPE SHARED CACHE STRING "How to build the slang library" FORCE)
    set(SLANG_ENABLE_GFX ON CACHE BOOL "Enable gfx targets" FORCE)
    
    FetchContent_MakeAvailable(slang)
    set(SLANG_FOUND TRUE)
    message(STATUS "Slang successfully built from source")
endif()

if(SLANG_FOUND)
    set(ASTRAL_HAS_SLANG TRUE)
    message(STATUS "Slang integration completed successfully")
else()
    message(WARNING 
        "Slang not found. SLANG shaders will not be supported.\n"
        "Please either:\n"
        "  1. Install Slang development libraries\n"
        "  2. Set slang_ROOT to your Slang installation path\n"
        "  3. Enable source build: -DASTRAL_VENDOR_SLANG=ON\n"
        "  4. Download Slang to ${EXTERNAL_DIR}/slang"
    )
endif()

# GLM Configuration
find_path(GLM_INCLUDE_DIR
    NAMES glm/glm.hpp
    PATHS ${EXTERNAL_DIR}/glm
    NO_DEFAULT_PATH
    DOC "GLM include directory"
)

if(NOT GLM_INCLUDE_DIR)
    find_package(glm QUIET)
    if(glm_FOUND)
        set(GLM_INCLUDE_DIR "${glm_INCLUDE_DIRS}")
        set(ASTRAL_HAS_GLM TRUE)
        message(STATUS "GLM found via system installation")
    else()
        message(WARNING "GLM not found. Some math functionality may be limited.")
        set(ASTRAL_HAS_GLM FALSE)
    endif()
else()
    set(ASTRAL_HAS_GLM TRUE)
    message(STATUS "GLM found at: ${GLM_INCLUDE_DIR}")
endif()

# Threading library
find_package(Threads REQUIRED)

# /**********************************************************************
#  * Astral Engine Library Target
#  **********************************************************************/
set(ASTRAL_ENGINE_SOURCES
    # Core
    Source/Core/Engine.cpp
    Source/Core/Logger.cpp
    Source/Core/FileLogger.cpp
    Source/Core/MemoryManager.cpp
    
    # Events
    Source/Events/EventManager.cpp
    
    # Platform Subsystem
    Source/Subsystems/Platform/PlatformSubsystem.cpp
    Source/Subsystems/Platform/Window.cpp
    Source/Subsystems/Platform/InputManager.cpp
    
    # ECS Subsystem
    Source/Subsystems/ECS/ECSSubsystem.cpp
)

set(ASTRAL_ENGINE_HEADERS
    # Core
    Source/Core/Engine.h
    Source/Core/ISubsystem.h
    Source/Core/Logger.h
    Source/Core/FileLogger.h
    Source/Core/MemoryManager.h
    
    # Events
    Source/Events/Event.h
    Source/Events/EventManager.h
    
    # Platform Subsystem
    Source/Subsystems/Platform/PlatformSubsystem.h
    Source/Subsystems/Platform/Window.h
    Source/Subsystems/Platform/InputManager.h
    Source/Subsystems/Platform/KeyCode.h
    
    # ECS
    Source/ECS/Components.h
    Source/Subsystems/ECS/ECSSubsystem.h
)

# Asset Subsystem (conditional)
if(ASTRAL_HAS_ASSIMP)
    list(APPEND ASTRAL_ENGINE_SOURCES
        Source/Subsystems/Asset/AssetSubsystem.cpp
        Source/Subsystems/Asset/AssetManager.cpp
        Source/Subsystems/Asset/AssetHandle.cpp
        Source/Subsystems/Asset/AssetRegistry.cpp
        Source/Subsystems/Asset/Model.cpp
    )
    
    list(APPEND ASTRAL_ENGINE_HEADERS
        Source/Subsystems/Asset/AssetSubsystem.h
        Source/Subsystems/Asset/AssetManager.h
        Source/Subsystems/Asset/AssetHandle.h
        Source/Subsystems/Asset/AssetRegistry.h
        Source/Subsystems/Asset/Model.h
        Source/Subsystems/Asset/ShaderProgram.h
    )
endif()

# Renderer Subsystem (conditional)
if(ASTRAL_HAS_VULKAN)
    list(APPEND ASTRAL_ENGINE_SOURCES
        Source/Subsystems/Renderer/RenderSubsystem.cpp
        Source/Subsystems/Renderer/VulkanRenderer.cpp
        Source/Subsystems/Renderer/Camera.cpp
        Source/Subsystems/Renderer/GraphicsDevice.cpp
        Source/Subsystems/Renderer/VulkanMemoryManager.cpp
        Source/Subsystems/Renderer/VulkanSynchronization.cpp
        Source/Subsystems/Renderer/VulkanUtils.cpp
        Source/Subsystems/Renderer/Core/VulkanInstance.cpp
        Source/Subsystems/Renderer/Core/VulkanDevice.cpp
        Source/Subsystems/Renderer/Core/VulkanSwapchain.cpp
        Source/Subsystems/Renderer/Core/VulkanFrameManager.cpp
        Source/Subsystems/Renderer/Shaders/VulkanShader.cpp
        Source/Subsystems/Renderer/Shaders/ShaderValidator.cpp
        Source/Subsystems/Renderer/Shaders/ShaderCompiler.cpp
        Source/Subsystems/Renderer/Shader/ShaderManager.cpp
        Source/Subsystems/Renderer/Commands/VulkanCommandPool.cpp
        Source/Subsystems/Renderer/Commands/VulkanPipeline.cpp
        Source/Subsystems/Renderer/Buffers/VulkanBuffer.cpp
        Source/Subsystems/Renderer/Buffers/VulkanMesh.cpp
        Source/Subsystems/Renderer/Buffers/VulkanTexture.cpp
        Source/Subsystems/Renderer/Texture/TextureManager.cpp
        Source/Subsystems/Renderer/Material/Material.cpp
    )
    
    list(APPEND ASTRAL_ENGINE_HEADERS
        Source/Subsystems/Renderer/RendererTypes.h
        Source/Subsystems/Renderer/IRenderer.h
        Source/Subsystems/Renderer/RenderSubsystem.h
        Source/Subsystems/Renderer/VulkanRenderer.h
        Source/Subsystems/Renderer/Camera.h
        Source/Subsystems/Renderer/GraphicsDevice.h
        Source/Subsystems/Renderer/VulkanMemoryManager.h
        Source/Subsystems/Renderer/VulkanSynchronization.h
        Source/Subsystems/Renderer/VulkanUtils.h
        Source/Subsystems/Renderer/Core/VulkanDevice.h
        Source/Subsystems/Renderer/Core/VulkanInstance.h
        Source/Subsystems/Renderer/Core/VulkanSwapchain.h
        Source/Subsystems/Renderer/Core/VulkanFrameManager.h
        Source/Subsystems/Renderer/Shaders/ShaderValidator.h
        Source/Subsystems/Renderer/Shaders/VulkanShader.h
        Source/Subsystems/Renderer/Commands/VulkanCommandPool.h
        Source/Subsystems/Renderer/Commands/VulkanPipeline.h
        Source/Subsystems/Renderer/Buffers/VulkanBuffer.h
        Source/Subsystems/Renderer/Buffers/VulkanMesh.h
        Source/Subsystems/Renderer/Buffers/VulkanTexture.h
        Source/Subsystems/Renderer/Shaders/ShaderCompiler.h
        Source/Subsystems/Renderer/Shader/ShaderManager.h
        Source/Subsystems/Renderer/Texture/TextureManager.h
        Source/Subsystems/Renderer/Material/Material.h
    )
endif()

# Create the main engine library
add_library(AstralEngine STATIC ${ASTRAL_ENGINE_SOURCES} ${ASTRAL_ENGINE_HEADERS})

# /**********************************************************************
#  * Target Configuration Function
#  **********************************************************************/
function(configure_astral_target target_name)
    # Compiler features
    target_compile_features(${target_name} PRIVATE cxx_std_20)

    # Include directories
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
        ${CMAKE_SOURCE_DIR}/External
        ${CMAKE_SOURCE_DIR}/External/stb
        $<$<BOOL:${GLM_INCLUDE_DIR}>:${GLM_INCLUDE_DIR}>
    )

    # Compile definitions
    target_compile_definitions(${target_name} PRIVATE
        # Version information
        ASTRAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        ASTRAL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        ASTRAL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        
        # Build configuration
        $<$<CONFIG:Debug>:ASTRAL_DEBUG>
        $<$<CONFIG:Release>:ASTRAL_RELEASE>
        $<$<CONFIG:RelWithDebInfo>:ASTRAL_RELEASE_WITH_DEBUG_INFO>
        
        # Platform definitions
        $<$<PLATFORM_ID:Windows>:PLATFORM_WINDOWS>
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX>
        $<$<PLATFORM_ID:Darwin>:PLATFORM_MACOS>
        
        # Windows-specific definitions
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        
        # Feature flags
        ASTRAL_USE_SDL3
        $<$<BOOL:${ASTRAL_HAS_VULKAN}>:ASTRAL_USE_VULKAN>
        $<$<BOOL:${ASTRAL_HAS_ASSIMP}>:ASTRAL_USE_ASSIMP>
        $<$<BOOL:${ASTRAL_HAS_GLM}>:ASTRAL_USE_GLM>
    )

    # Vulkan-specific definitions
    if(ASTRAL_HAS_VULKAN)
        target_compile_definitions(${target_name} PRIVATE
            VULKAN_HPP_NO_EXCEPTIONS
            VULKAN_HPP_TYPESAFE_CONVERSION
            $<$<BOOL:${ASTRAL_VULKAN_VALIDATION}>:$<$<CONFIG:Debug>:ASTRAL_VULKAN_VALIDATION_LAYERS>>
        )
    endif()

    # Link libraries
    target_link_libraries(${target_name} PRIVATE
        Threads::Threads
    )

    # SDL3 linking
    if(TARGET SDL3::SDL3)
        target_link_libraries(${target_name} PRIVATE SDL3::SDL3)
        
        if(WIN32 AND ASTRAL_LINK_SDL3MAIN AND TARGET SDL3::SDL3main)
            target_link_libraries(${target_name} PRIVATE SDL3::SDL3main)
        endif()
    elseif(ASTRAL_USE_LOCAL_SDL3)
        target_link_libraries(${target_name} PRIVATE SDL3)
        
        if(WIN32 AND ASTRAL_LINK_SDL3MAIN AND TARGET SDL3main)
            target_link_libraries(${target_name} PRIVATE SDL3main)
        endif()
    endif()

    # Vulkan linking
    if(ASTRAL_HAS_VULKAN)
        if(TARGET Vulkan::Vulkan)
            target_link_libraries(${target_name} PRIVATE Vulkan::Vulkan)
        elseif(TARGET vulkan)
            target_link_libraries(${target_name} PRIVATE vulkan)
        endif()
        
        if(TARGET VulkanMemoryAllocator::VulkanMemoryAllocator)
            target_link_libraries(${target_name} PRIVATE VulkanMemoryAllocator::VulkanMemoryAllocator)
        endif()
    endif()

    # Assimp linking
    if(ASTRAL_HAS_ASSIMP)
        if(TARGET assimp::assimp)
            target_link_libraries(${target_name} PRIVATE assimp::assimp)
        elseif(TARGET assimp)
            target_link_libraries(${target_name} PRIVATE assimp)
        endif()
    endif()

    # SLANG linking
    if(ASTRAL_HAS_SLANG)
        if(TARGET slang::slang)
            target_link_libraries(${target_name} PRIVATE slang::slang)
        elseif(TARGET slang)
            target_link_libraries(${target_name} PRIVATE slang)
        endif()
    endif()

    # Copy SDL3 DLL on Windows
    if(WIN32)
        if(ASTRAL_USE_PREBUILT_SDL3 AND DEFINED SDL3_DLL_PATH)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SDL3_DLL_PATH}"
                    $<TARGET_FILE_DIR:${target_name}>
                COMMENT "Copying pre-built SDL3 DLL to output directory"
                VERBATIM
            )
        elseif(TARGET SDL3::SDL3)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:SDL3::SDL3>
                    $<TARGET_FILE_DIR:${target_name}>
                COMMENT "Copying SDL3 DLL to output directory"
                VERBATIM
            )
        elseif(ASTRAL_USE_LOCAL_SDL3 AND TARGET SDL3)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:SDL3>
                    $<TARGET_FILE_DIR:${target_name}>
                COMMENT "Copying local SDL3 DLL to output directory"
                VERBATIM
            )
        endif()
    endif()
endfunction()

# Configure the main engine library
configure_astral_target(AstralEngine)

# IDE organization
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE ${CMAKE_SOURCE_DIR}/Source PREFIX "Source" FILES ${ASTRAL_ENGINE_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR}/Source PREFIX "Headers" FILES ${ASTRAL_ENGINE_HEADERS})

# /**********************************************************************
#  * Shader Compilation System
#  **********************************************************************/
if(ASTRAL_HAS_VULKAN)
    # Find glslangValidator executable (better for #include support)
    find_program(Vulkan_GLSLANG_VALIDATOR_EXECUTABLE
        NAMES glslangValidator
        PATHS
            "$ENV{VULKAN_SDK}/Bin"
            "$ENV{VULKAN_SDK}/bin"
            "/usr/bin"
            "/usr/local/bin"
            "/opt/local/bin"
        DOC "Path to glslangValidator shader compiler"
    )
    
    if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
        message(WARNING
            "glslangValidator shader compiler not found. GLSL shaders won't be compiled.\n"
            "Please install Vulkan SDK or ensure glslangValidator is in PATH."
        )
    else()
        message(STATUS "glslangValidator found at: ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}")
    endif()
    
    # Find slangc executable (SLANG compiler)
    find_program(SLANG_COMPILER_EXECUTABLE
        NAMES slangc
        PATHS
            "${slang_BINARY_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/slang-build/bin"
            "/usr/bin"
            "/usr/local/bin"
            "/opt/local/bin"
        DOC "Path to SLANG shader compiler"
    )
    
    if(NOT SLANG_COMPILER_EXECUTABLE)
        message(WARNING
            "slangc shader compiler not found. SLANG shaders won't be compiled.\n"
            "Please ensure SLANG is built correctly or slangc is in PATH."
        )
    else()
        message(STATUS "slangc found at: ${SLANG_COMPILER_EXECUTABLE}")
    endif()
    
    # GLSL shader compilation function
    function(compile_glsl_shaders TARGET_NAME SHADER_DIR)
        if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
            message(WARNING "glslangValidator not found, GLSL shaders won't be compiled")
            return()
        endif()
        
        file(GLOB_RECURSE GLSL_SHADERS 
             "${SHADER_DIR}/*.vert" 
             "${SHADER_DIR}/*.frag" 
             "${SHADER_DIR}/*.comp"
             "${SHADER_DIR}/*.geom"
             "${SHADER_DIR}/*.tesc"
             "${SHADER_DIR}/*.tese")
        
        set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
        file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})
        
        foreach(SHADER ${GLSL_SHADERS})
            get_filename_component(SHADER_NAME ${SHADER} NAME)
            set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
            
            # glslangValidator automatically handles #include directives
            # Set working directory to the shader's directory for relative includes
            get_filename_component(SHADER_DIR_PATH ${SHADER} DIRECTORY)
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E chdir ${SHADER_DIR_PATH} 
                    ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} 
                    -V ${SHADER} -o ${SHADER_OUTPUT}
                DEPENDS ${SHADER}
                COMMENT "Compiling GLSL shader ${SHADER_NAME}"
                WORKING_DIRECTORY ${SHADER_DIR_PATH}
            )
            
            list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
        endforeach()
        
        add_custom_target(${TARGET_NAME}_glsl_shaders DEPENDS ${COMPILED_SHADERS})
    endfunction()
    
    # SLANG shader compilation function
    function(compile_slang_shaders TARGET_NAME SHADER_DIR)
        if(NOT ASTRAL_HAS_SLANG)
            message(WARNING "Slang not found, SLANG shaders won't be compiled")
            return()
        endif()
        
        # Get slangc executable from the target or find it
        if(TARGET slang::slangc)
            set(SLANG_COMPILER_EXECUTABLE $<TARGET_FILE:slang::slangc>)
        elseif(TARGET slangc)
            set(SLANG_COMPILER_EXECUTABLE $<TARGET_FILE:slangc>)
        else()
            # Try to find it in the build directory
            find_program(SLANG_COMPILER_EXECUTABLE
                NAMES slangc
                PATHS
                    "${CMAKE_BINARY_DIR}/_deps/slang-build/bin"
                    "${CMAKE_BINARY_DIR}/bin"
                    "${CMAKE_BINARY_DIR}/_deps/slang-build/slang-2025.16.1-windows-x86_64/bin"
                NO_DEFAULT_PATH
            )
        endif()
        
        if(NOT SLANG_COMPILER_EXECUTABLE)
            message(WARNING "slangc not found, SLANG shaders won't be compiled")
            return()
        endif()
        
        message(STATUS "Using slangc at: ${SLANG_COMPILER_EXECUTABLE}")
        
        file(GLOB_RECURSE SLANG_SHADERS 
             "${SHADER_DIR}/*.slang")
        
        set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
        file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})
        
        foreach(SHADER ${SLANG_SHADERS})
            get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
            get_filename_component(SHADER_EXT ${SHADER} EXT)
            
            # Determine shader stage from file extension
            if(SHADER_EXT MATCHES "\\.(vert|frag|comp|geom|tesc|tese)\\.slang")
                string(REGEX REPLACE "\\.slang$" "" STAGE_EXT ${SHADER_EXT})
            else()
                # Default to fragment shader if no stage extension
                set(STAGE_EXT ".frag")
            endif()
            
            set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}${STAGE_EXT}.spv")
            
            # Compile SLANG shader to SPIR-V
            get_filename_component(SHADER_DIR_PATH ${SHADER} DIRECTORY)
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E chdir ${SHADER_DIR_PATH}
                    ${SLANG_COMPILER_EXECUTABLE}
                    -target spirv-vulkan1.3
                    -stage ${STAGE_EXT}
                    -o ${SHADER_OUTPUT}
                    ${SHADER}
                DEPENDS ${SHADER}
                COMMENT "Compiling SLANG shader ${SHADER_NAME}${STAGE_EXT}"
                WORKING_DIRECTORY ${SHADER_DIR_PATH}
            )
            
            list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
        endforeach()
        
        add_custom_target(${TARGET_NAME}_slang_shaders DEPENDS ${COMPILED_SHADERS})
    endfunction()
    
    # Combined shader compilation function
    function(compile_all_shaders TARGET_NAME SHADER_DIR)
        compile_glsl_shaders(${TARGET_NAME} ${SHADER_DIR})
        compile_slang_shaders(${TARGET_NAME} ${SHADER_DIR})
        
        # Create a combined target that depends on both GLSL and SLANG shaders
        add_custom_target(${TARGET_NAME}_shaders)
        add_dependencies(${TARGET_NAME}_shaders ${TARGET_NAME}_glsl_shaders ${TARGET_NAME}_slang_shaders)
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)
    endfunction()

    # Compile all shaders for the engine
    compile_all_shaders(AstralEngine "${CMAKE_SOURCE_DIR}/Assets/Shaders")
endif()

# /**********************************************************************
#  * Examples and Tests
#  **********************************************************************/
if(ASTRAL_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif()

if(ASTRAL_BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(Tests)
    else()
        message(WARNING "GTest not found. Tests will not be built.")
    endif()
endif()

# /**********************************************************************
#  * SDL3 Test Application
#  **********************************************************************/
if(EXISTS "${CMAKE_SOURCE_DIR}/Source/test_sdl3.cpp")
    add_executable(AstralEngine_SDL3Test Source/test_sdl3.cpp)
    target_link_libraries(AstralEngine_SDL3Test PRIVATE AstralEngine)
    configure_astral_target(AstralEngine_SDL3Test)
    
    set_target_properties(AstralEngine_SDL3Test PROPERTIES
        OUTPUT_NAME "AstralEngine_SDL3Test$<$<CONFIG:Debug>:_Debug>"
        DESCRIPTION "Astral Engine SDL3 Test Application"
        VERSION ${PROJECT_VERSION}
        $<$<PLATFORM_ID:Windows>:WIN32_EXECUTABLE OFF>
        $<$<CONFIG:Debug>:DEBUG_POSTFIX "_d">
        $<$<CONFIG:RelWithDebInfo>:RELWITHDEBINFO_POSTFIX "_rd">
    )
    
    # Copy assets for SDL3 Test
    add_custom_command(TARGET AstralEngine_SDL3Test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:AstralEngine_SDL3Test>/Assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Assets
            $<TARGET_FILE_DIR:AstralEngine_SDL3Test>/Assets
        COMMENT "Copying assets to SDL3 Test output directory"
    )
    
    # Copy compiled shaders
    if(ASTRAL_HAS_VULKAN)
        add_custom_command(TARGET AstralEngine_SDL3Test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${CMAKE_BINARY_DIR}/shaders"
                "$<TARGET_FILE_DIR:AstralEngine_SDL3Test>/Assets/Shaders"
            COMMENT "Copying all compiled shaders to SDL3 Test output directory"
        )
    endif()
    
    message(STATUS "SDL3 Test Application target created")
endif()

# /**********************************************************************
#  * Build Summary
#  **********************************************************************/
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "Astral Engine Build Configuration Summary")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Build Examples: ${ASTRAL_BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${ASTRAL_BUILD_TESTS}")
message(STATUS "  Vendor SDL3: ${ASTRAL_VENDOR_SDL3}")
message(STATUS "  Vendor Vulkan: ${ASTRAL_VENDOR_VULKAN}")
message(STATUS "  Vendor Assimp: ${ASTRAL_VENDOR_ASSIMP}")
message(STATUS "  Warnings as Errors: ${ASTRAL_WARNINGS_AS_ERRORS}")
message(STATUS "  Vulkan Support: ${ASTRAL_ENABLE_VULKAN}")
message(STATUS "  Vulkan Validation: ${ASTRAL_VULKAN_VALIDATION}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  SDL3: ${ASTRAL_HAS_SDL3}")
message(STATUS "  Vulkan: ${ASTRAL_HAS_VULKAN}")
message(STATUS "  Assimp: ${ASTRAL_HAS_ASSIMP}")
message(STATUS "  GLM: ${ASTRAL_HAS_GLM}")
message(STATUS "")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "==========================================")
