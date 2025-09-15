# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Advanced 3D Game Engine
#  *
#  * CMake Configuration File
#  * Purpose: Professional SDL3 integration with modern C++20 support
#  * Author: Astral Engine Development Team
#  * Version: 0.1.0-alpha
#  * Date: 2025-09-11
#  **********************************************************************/

cmake_minimum_required(VERSION 3.24)
project(AstralEngine VERSION 0.1.0 LANGUAGES CXX)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type validation
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Build options
option(ASTRAL_BUILD_EXAMPLES "Build sample applications" OFF)
option(ASTRAL_VENDOR_SDL3 "Fetch and build SDL3 from source if not found" OFF)
option(ASTRAL_LINK_SDL3MAIN "Link SDL3main on Windows for WinMain wrapping" ON)
option(ASTRAL_WARNINGS_AS_ERRORS "Treat warnings as errors in engine code" OFF)
option(ASTRAL_ENABLE_PROFILING "Enable profiling support" OFF)
option(ASTRAL_ENABLE_TESTS "Enable unit tests" OFF)
option(ASTRAL_ENABLE_VULKAN "Enable Vulkan graphics API support" ON)
option(ASTRAL_VULKAN_VALIDATION "Enable Vulkan validation layers in debug builds" ON)
option(ASTRAL_VENDOR_VULKAN "Fetch and build Vulkan dependencies from source if not found" OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# External dependencies directory
set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/External)

# Enable FetchContent for dependency management
include(FetchContent)

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
        /utf-8                 # Use UTF-8 for source and execution character sets
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

# Vulkan-specific compiler optimizations and definitions
if(ASTRAL_ENABLE_VULKAN)
    # Platform-independent Vulkan optimizations
    add_compile_definitions(
        $<$<CONFIG:Release>:VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1>
        $<$<CONFIG:Debug>:VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1>
    )
    
    # Vulkan validation layers configuration
    if(ASTRAL_VULKAN_VALIDATION)
        add_compile_definitions(
            $<$<CONFIG:Debug>:ASTRAL_VULKAN_VALIDATION_LAYERS>
            $<$<CONFIG:Debug>:VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=1>
        )
        
        # Debug-specific Vulkan settings
        if(MSVC)
            add_compile_options($<$<CONFIG:Debug>:/DVK_ENABLE_BETA_EXTENSIONS>)
        else()
            add_compile_options($<$<CONFIG:Debug>:-DVK_ENABLE_BETA_EXTENSIONS>)
        endif()
    endif()
    
    # Platform-specific Vulkan optimizations
    if(WIN32)
        # Windows-specific Vulkan optimizations
        add_compile_definitions(
            $<$<CONFIG:Release>:VK_USE_PLATFORM_WIN32_KHR>
            $<$<CONFIG:Debug>:VK_USE_PLATFORM_WIN32_KHR>
        )
        
        # Enable Windows-specific Vulkan extensions
        if(MSVC)
            add_compile_options(
                $<$<CONFIG:Release>:/arch:AVX2>
                $<$<CONFIG:RelWithDebInfo>:/arch:AVX2>
            )
        endif()
    elseif(UNIX AND NOT APPLE)
        # Linux-specific Vulkan optimizations
        add_compile_definitions(
            $<$<CONFIG:Release>:VK_USE_PLATFORM_XCB_KHR>
            $<$<CONFIG:Debug>:VK_USE_PLATFORM_XCB_KHR>
        )
        
        # GCC/Clang specific optimizations for Vulkan
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            add_compile_options(
                $<$<CONFIG:Release>:-march=native>
                $<$<CONFIG:Release>:-mtune=native>
            )
        endif()
    endif()
endif()

# /**********************************************************************
#  * SDL3 Detection and Configuration
#  **********************************************************************/
set(SDL3_ROOT "" CACHE PATH "Path to SDL3 installation directory")
set(ASTRAL_SDL3_VERSION "3.2.22" CACHE STRING "Target SDL3 version")

# Enhanced SDL3 detection with multiple fallback strategies
message(STATUS "Searching for SDL3...")

# Strategy 1: Check for pre-built SDL3 in External directory first
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
endif()

# Strategy 3: Try pkgconfig if available
if(NOT SDL3_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL3_PKG QUIET sdl3)
        if(SDL3_PKG_FOUND)
            set(SDL3_FOUND TRUE)
            add_library(SDL3::SDL3 INTERFACE IMPORTED)
            target_link_libraries(SDL3::SDL3 INTERFACE ${SDL3_PKG_LIBRARIES})
            target_include_directories(SDL3::SDL3 INTERFACE ${SDL3_PKG_INCLUDE_DIRS})
            target_compile_options(SDL3::SDL3 INTERFACE ${SDL3_PKG_CFLAGS_OTHER})
            message(STATUS "SDL3 found via pkg-config")
        endif()
    endif()
endif()

# Strategy 4: FetchContent from source if enabled
if(NOT SDL3_FOUND AND ASTRAL_VENDOR_SDL3)
    message(STATUS "Fetching SDL3 from source (version: ${ASTRAL_SDL3_VERSION})...")
    
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-${ASTRAL_SDL3_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    
    # Configure SDL3 build options for optimal performance
    set(SDL_SHARED ON CACHE BOOL "Build SDL3 as shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL3 as static library" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL3 test library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
    set(SDL_INSTALL_TESTS OFF CACHE BOOL "Install SDL3 tests" FORCE)
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "Disable SDL3 install" FORCE)
    
    # Performance optimizations
    set(SDL_ASSERTIONS disabled CACHE STRING "SDL assertion level" FORCE)
    
    FetchContent_MakeAvailable(SDL3)
    set(SDL3_FOUND TRUE)
    message(STATUS "SDL3 successfully built from source")
endif()

# Final validation and error reporting
if(NOT SDL3_FOUND)
    message(FATAL_ERROR
        "\n========================================\n"
        "SDL3 Integration Failed!\n"
        "\n"
        "SDL3 ${ASTRAL_SDL3_VERSION} could not be found or built.\n"
        "\n"
        "Solutions:\n"
        "  1. Install SDL3 development libraries:\n"
        "     - Windows: Download from https://github.com/libsdl-org/SDL/releases\n"
        "     - Linux: sudo apt install libsdl3-dev (when available)\n"
        "     - macOS: brew install sdl3 (when available)\n"
        "\n"
        "  2. Set SDL3_ROOT to your SDL3 installation path\n"
        "\n"
        "  3. Enable source build: -DASTRAL_VENDOR_SDL3=ON\n"
        "\n"
        "  4. Use vcpkg: vcpkg install sdl3\n"
        "\n"
        "For detailed instructions, see SDL3_INTEGRATION_GUIDE.md\n"
        "========================================\n"
    )
endif()

# Handle pre-built SDL3 if detected
if(ASTRAL_USE_PREBUILT_SDL3)
    message(STATUS "Configuring pre-built SDL3 from External/SDL3...")
    
    # Determine architecture-specific library path
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(SDL3_ARCH_DIR "x64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
        set(SDL3_ARCH_DIR "x86")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(SDL3_ARCH_DIR "arm64")
    else()
        set(SDL3_ARCH_DIR "x64")  # Default fallback
    endif()
    
    set(SDL3_LIB_DIR "${EXTERNAL_DIR}/SDL3/lib/${SDL3_ARCH_DIR}")
    
    # Verify the library exists
    if(NOT EXISTS "${SDL3_LIB_DIR}/SDL3.lib")
        message(FATAL_ERROR "SDL3.lib not found at ${SDL3_LIB_DIR}")
    endif()
    
    # Create imported target for SDL3
    add_library(SDL3::SDL3 SHARED IMPORTED)
    set_target_properties(SDL3::SDL3 PROPERTIES
        IMPORTED_LOCATION "${SDL3_LIB_DIR}/SDL3.dll"
        IMPORTED_IMPLIB "${SDL3_LIB_DIR}/SDL3.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${EXTERNAL_DIR}/SDL3/include"
    )
    
    # Store DLL path for copying
    set(SDL3_DLL_PATH "${SDL3_LIB_DIR}/SDL3.dll")
    
    message(STATUS "Pre-built SDL3 configured successfully (${SDL3_ARCH_DIR})")
endif()

# Handle source SDL3 build if detected
if(ASTRAL_USE_LOCAL_SDL3)
    message(STATUS "Building local SDL3 from External/SDL3...")
    
    # Configure SDL3 build options for optimal performance
    set(SDL_SHARED ON CACHE BOOL "Build SDL3 as shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL3 as static library" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL3 test library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
    set(SDL_INSTALL_TESTS OFF CACHE BOOL "Install SDL3 tests" FORCE)
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "Disable SDL3 install" FORCE)
    
    # Performance optimizations
    set(SDL_ASSERTIONS disabled CACHE STRING "SDL assertion level" FORCE)
    
    # Add SDL3 subdirectory
    add_subdirectory(${EXTERNAL_DIR}/SDL3 EXCLUDE_FROM_ALL)
    
    message(STATUS "Local SDL3 build configured successfully")
endif()

# SDL3 configuration logging and validation
if(TARGET SDL3::SDL3)
    get_target_property(SDL3_TYPE SDL3::SDL3 TYPE)
    message(STATUS "SDL3 target type: ${SDL3_TYPE}")
    
    # Try to get version information
    if(SDL3_VERSION)
        message(STATUS "SDL3 version: ${SDL3_VERSION}")
    elseif(SDL3_PKG_VERSION)
        message(STATUS "SDL3 version: ${SDL3_PKG_VERSION}")
    endif()
elseif(ASTRAL_USE_PREBUILT_SDL3)
    message(STATUS "SDL3 using pre-built libraries from External/SDL3")
elseif(ASTRAL_USE_LOCAL_SDL3)
    message(STATUS "SDL3 built from local source in External/SDL3")
else()
    message(STATUS "SDL3 found via FetchContent (built from source)")
endif()

set(ASTRAL_HAS_SDL3 TRUE)

# /**********************************************************************
#  * GLM Mathematics Library
#  **********************************************************************/
find_path(GLM_INCLUDE_DIR
    NAMES glm/glm.hpp
    PATHS ${EXTERNAL_DIR}/glm ${EXTERNAL_DIR}/glm/glm
    NO_DEFAULT_PATH
    DOC "GLM include directory"
)

if(NOT GLM_INCLUDE_DIR)
    # Fallback: try system installation
    find_package(glm QUIET)
    if(NOT glm_FOUND)
        message(WARNING 
            "GLM library not found. Please either:\n"
            "  1. Download GLM to ${EXTERNAL_DIR}/glm\n"
            "  2. Install system-wide GLM package\n"
            "Download from: https://github.com/g-truc/glm\n"
            "\n"
            "Continuing without GLM (some features may be limited)..."
        )
        set(ASTRAL_HAS_GLM FALSE)
    else()
        set(ASTRAL_HAS_GLM TRUE)
    endif()
else()
    message(STATUS "GLM found at: ${GLM_INCLUDE_DIR}")
    set(ASTRAL_HAS_GLM TRUE)
endif()

# /**********************************************************************
#  * Vulkan 1.4 Integration
#  **********************************************************************/
if(ASTRAL_ENABLE_VULKAN)
    message(STATUS "Configuring Vulkan 1.4 support...")
    
    # Vulkan SDK version configuration
    set(ASTRAL_VULKAN_VERSION "1.4" CACHE STRING "Target Vulkan SDK version")
    set(ASTRAL_VULKAN_SDK_ROOT "" CACHE PATH "Path to Vulkan SDK installation directory")
    
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
            set(VULKAN_ARCH_DIR "x64")  # Default fallback
        endif()
        
        set(VULKAN_LIB_DIR "${EXTERNAL_DIR}/Vulkan/lib/${VULKAN_ARCH_DIR}")
        
        # Create imported target for Vulkan
        add_library(Vulkan::Vulkan INTERFACE IMPORTED)
        target_include_directories(Vulkan::Vulkan INTERFACE "${EXTERNAL_DIR}/Vulkan/include")
        target_link_directories(Vulkan::Vulkan INTERFACE "${VULKAN_LIB_DIR}")
        
        # Link Vulkan libraries
        if(WIN32)
            target_link_libraries(Vulkan::Vulkan INTERFACE vulkan-1 shaderc_shared)
        else()
            target_link_libraries(Vulkan::Vulkan INTERFACE vulkan shaderc)
        endif()
        
        message(STATUS "Pre-built Vulkan SDK configured successfully (${VULKAN_ARCH_DIR})")
    endif()
    
    # Strategy 2: Try to find Vulkan SDK via system installation
    if(NOT VULKAN_FOUND)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            set(VULKAN_FOUND TRUE)
            message(STATUS "Vulkan SDK found via system installation")
        endif()
    endif()
    
    # Strategy 3: Try to find Vulkan via environment variable
    if(NOT VULKAN_FOUND AND DEFINED ENV{VULKAN_SDK})
        set(VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}")
        if(EXISTS "${VULKAN_SDK_ROOT}/Include/vulkan" AND EXISTS "${VULKAN_SDK_ROOT}/Lib")
            set(VULKAN_FOUND TRUE)
            set(ASTRAL_USE_SYSTEM_VULKAN TRUE)
            
            # Create imported target for system Vulkan
            add_library(Vulkan::Vulkan INTERFACE IMPORTED)
            target_include_directories(Vulkan::Vulkan INTERFACE "${VULKAN_SDK_ROOT}/Include")
            target_link_directories(Vulkan::Vulkan INTERFACE "${VULKAN_SDK_ROOT}/Lib")
            
            if(WIN32)
                target_link_libraries(Vulkan::Vulkan INTERFACE vulkan-1 shaderc_shared)
            else()
                target_link_libraries(Vulkan::Vulkan INTERFACE vulkan shaderc)
            endif()
            
            message(STATUS "Vulkan SDK found via VULKAN_SDK environment variable")
        endif()
    endif()
    
    # Strategy 4: FetchContent for Vulkan dependencies if enabled
    if(NOT VULKAN_FOUND AND ASTRAL_VENDOR_VULKAN)
        message(STATUS "Fetching Vulkan dependencies from source...")
        
        # Vulkan Headers
        FetchContent_Declare(
            VulkanHeaders
            GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
            GIT_TAG v1.4.0
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        
        # Vulkan Memory Allocator (VMA)
        FetchContent_Declare(
            VulkanMemoryAllocator
            GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
            GIT_TAG v3.1.0
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        
        # GLM (if not already found)
        if(NOT ASTRAL_HAS_GLM)
            FetchContent_Declare(
                glm
                GIT_REPOSITORY https://github.com/g-truc/glm.git
                GIT_TAG 0.9.9.8
                GIT_SHALLOW TRUE
                GIT_PROGRESS TRUE
            )
        endif()
        
        FetchContent_MakeAvailable(VulkanHeaders VulkanMemoryAllocator)
        if(NOT ASTRAL_HAS_GLM)
            FetchContent_MakeAvailable(glm)
        endif()
        
        set(VULKAN_FOUND TRUE)
        message(STATUS "Vulkan dependencies fetched successfully")
    endif()
    
    # Final validation and error reporting
    if(NOT VULKAN_FOUND)
        message(WARNING
            "\n========================================\n"
            "Vulkan 1.4 Integration Warning!\n"
            "\n"
            "Vulkan SDK could not be found or built.\n"
            "\n"
            "Solutions:\n"
            "  1. Install Vulkan SDK:\n"
            "     - Windows: Download from https://vulkan.lunarg.com/\n"
            "     - Linux: sudo apt install vulkan-tools libvulkan-dev\n"
            "     - macOS: brew install vulkan-headers vulkan-loader\n"
            "\n"
            "  2. Set VULKAN_SDK environment variable\n"
            "\n"
            "  3. Download Vulkan SDK to ${EXTERNAL_DIR}/Vulkan\n"
            "\n"
            "  4. Enable source build: -DASTRAL_VENDOR_VULKAN=ON\n"
            "\n"
            "Continuing without Vulkan support...\n"
            "========================================\n"
        )
        set(ASTRAL_ENABLE_VULKAN FALSE)
    else()
        set(ASTRAL_HAS_VULKAN TRUE)
        message(STATUS "Vulkan 1.4 support configured successfully")
    endif()
endif()

# /**********************************************************************
#  * Engine Source Files
#  **********************************************************************/
set(ENGINE_SOURCES
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
    
    # Asset Subsystem
    Source/Subsystems/Asset/AssetSubsystem.cpp
    Source/Subsystems/Asset/AssetManager.cpp
    
    # Renderer Subsystem
    Source/Subsystems/Renderer/VulkanRenderer.cpp
    Source/Subsystems/Renderer/RendererFactory.cpp
    Source/Subsystems/Renderer/RenderCommandQueue.cpp
    Source/Subsystems/Renderer/Camera.cpp
    Source/Subsystems/Renderer/Vulkan/VulkanGraphicsContext.cpp
    Source/Subsystems/Renderer/Vulkan/VulkanCommandBufferManager.cpp
    Source/Subsystems/Renderer/Core/VulkanInstance.cpp
    Source/Subsystems/Renderer/Core/VulkanDevice.cpp
    Source/Subsystems/Renderer/Core/VulkanSwapchain.cpp
    Source/Subsystems/Renderer/Shaders/VulkanShader.cpp
    Source/Subsystems/Renderer/Shaders/ShaderValidator.cpp
    Source/Subsystems/Renderer/Commands/VulkanCommandPool.cpp
    Source/Subsystems/Renderer/Commands/VulkanPipeline.cpp
    Source/Subsystems/Renderer/Buffers/VulkanBuffer.cpp
    
    # Main entry point
    Source/main.cpp
)

set(ENGINE_HEADERS
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
    
    # Asset Subsystem
    Source/Subsystems/Asset/AssetSubsystem.h
    Source/Subsystems/Asset/AssetManager.h
    
    # ECS
    Source/ECS/Components.h
)

# /**********************************************************************
#  * Main Engine Target
#  **********************************************************************/
add_executable(AstralEngine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Compiler features
target_compile_features(AstralEngine PRIVATE cxx_std_20)

# Include directories
target_include_directories(AstralEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    $<$<BOOL:${GLM_INCLUDE_DIR}>:${GLM_INCLUDE_DIR}>
    $<$<BOOL:${ASTRAL_HAS_VULKAN}>:${CMAKE_SOURCE_DIR}/External/vulkan/include>
)

# Compile definitions
target_compile_definitions(AstralEngine PRIVATE
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
    $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX>
    $<$<PLATFORM_ID:Darwin>:PLATFORM_MACOS>
    
    # Windows-specific definitions
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    
    # SDL3 integration
    ASTRAL_USE_SDL3
    
    # Vulkan integration
    $<$<BOOL:${ASTRAL_HAS_VULKAN}>:ASTRAL_USE_VULKAN>
    $<$<BOOL:${ASTRAL_HAS_VULKAN}>:VULKAN_HPP_NO_EXCEPTIONS>
    $<$<BOOL:${ASTRAL_HAS_VULKAN}>:VULKAN_HPP_TYPESAFE_CONVERSION>
)

# Link libraries
target_link_libraries(AstralEngine PRIVATE
    # Threading
    Threads::Threads
    
    # Vulkan libraries
    $<$<TARGET_EXISTS:Vulkan::Vulkan>:Vulkan::Vulkan>
    $<$<TARGET_EXISTS:vulkan>::vulkan>
)

# SDL3 linking
if(TARGET SDL3::SDL3)
    target_link_libraries(AstralEngine PRIVATE SDL3::SDL3)
    
    # Link SDL3main on Windows
    if(WIN32 AND ASTRAL_LINK_SDL3MAIN AND TARGET SDL3::SDL3main)
        target_link_libraries(AstralEngine PRIVATE SDL3::SDL3main)
    endif()
elseif(ASTRAL_USE_LOCAL_SDL3)
    # Local SDL3 build
    target_link_libraries(AstralEngine PRIVATE SDL3)
    
    # Link SDL3main on Windows for local build
    if(WIN32 AND ASTRAL_LINK_SDL3MAIN AND TARGET SDL3main)
        target_link_libraries(AstralEngine PRIVATE SDL3main)
    endif()
else()
    # FetchContent SDL3
    target_link_libraries(AstralEngine PRIVATE SDL3)
endif()

# GLM linking
if(TARGET glm::glm)
    target_link_libraries(AstralEngine PRIVATE glm::glm)
endif()

# Threading library
find_package(Threads REQUIRED)

# IDE organization
source_group(TREE ${CMAKE_SOURCE_DIR}/Source PREFIX "Source" FILES ${ENGINE_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR}/Source PREFIX "Headers" FILES ${ENGINE_HEADERS})

# Target properties
set_target_properties(AstralEngine PROPERTIES
    OUTPUT_NAME "AstralEngine$<$<CONFIG:Debug>:_Debug>"
    DESCRIPTION "Astral Engine v${PROJECT_VERSION} - Advanced 3D Game Engine"
    VERSION ${PROJECT_VERSION}
    
    # Windows specific properties
    $<$<PLATFORM_ID:Windows>:WIN32_EXECUTABLE OFF>
    
    # Debug symbols
    $<$<CONFIG:Debug>:DEBUG_POSTFIX "_d">
    $<$<CONFIG:RelWithDebInfo>:RELWITHDEBINFO_POSTFIX "_rd">
)

# /**********************************************************************
#  * Post-Build Steps
#  **********************************************************************/
# Copy assets
add_custom_command(TARGET AstralEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:AstralEngine>/Assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:AstralEngine>/Assets
    COMMENT "Copying assets to output directory"
)

# Copy compiled shaders to output directory
add_custom_command(TARGET AstralEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:AstralEngine>/Assets/Shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
        $<TARGET_FILE_DIR:AstralEngine>/Assets/Shaders/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
        $<TARGET_FILE_DIR:AstralEngine>/Assets/Shaders/
    COMMENT "Copying compiled shaders to output directory"
)

# Copy SDL3 DLL on Windows (if needed)
if(WIN32)
    if(ASTRAL_USE_PREBUILT_SDL3 AND DEFINED SDL3_DLL_PATH)
        add_custom_command(TARGET AstralEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL3_DLL_PATH}"
                $<TARGET_FILE_DIR:AstralEngine>
            COMMENT "Copying pre-built SDL3 DLL to output directory"
            VERBATIM
        )
    elseif(TARGET SDL3::SDL3)
        add_custom_command(TARGET AstralEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE_DIR:AstralEngine>
            COMMENT "Copying SDL3 DLL to output directory"
            VERBATIM
        )
    elseif(ASTRAL_USE_LOCAL_SDL3 AND TARGET SDL3)
        add_custom_command(TARGET AstralEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3>
                $<TARGET_FILE_DIR:AstralEngine>
            COMMENT "Copying local SDL3 DLL to output directory"
            VERBATIM
        )
    endif()
endif()

# /**********************************************************************
#  * Optional: Sandbox Example Application
#  **********************************************************************/
if(ASTRAL_BUILD_EXAMPLES)
    add_subdirectory(Examples OPTIONAL)
endif()

# /**********************************************************************
#  * Optional: Unit Tests
#  **********************************************************************/
if(ASTRAL_ENABLE_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(Tests OPTIONAL)
    endif()
endif()

# /**********************************************************************
#  * Shader Compilation System
#  **********************************************************************/
if(ASTRAL_HAS_VULKAN)
    # Shader compilation function
    function(compile_vulkan_shaders TARGET_NAME SHADER_DIR)
        if(NOT Vulkan_GLSLC_EXECUTABLE)
            message(WARNING "GLSLC not found, shaders won't be compiled")
            return()
        endif()
        
        file(GLOB_RECURSE SHADER_SOURCES 
             "${SHADER_DIR}/*.vert" 
             "${SHADER_DIR}/*.frag" 
             "${SHADER_DIR}/*.comp"
             "${SHADER_DIR}/*.geom"
             "${SHADER_DIR}/*.tesc"
             "${SHADER_DIR}/*.tese")
        
        set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
        file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})
        
        foreach(SHADER ${SHADER_SOURCES})
            get_filename_component(SHADER_NAME ${SHADER} NAME)
            set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
            
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_OUTPUT}
                DEPENDS ${SHADER}
                COMMENT "Compiling shader ${SHADER_NAME}"
            )
            
            list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT})
        endforeach()
        
        add_custom_target(${TARGET_NAME}_shaders DEPENDS ${COMPILED_SHADERS})
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)
    endfunction()

    # Shader'ları compile et
    compile_vulkan_shaders(AstralEngine "${CMAKE_SOURCE_DIR}/Assets/Shaders")
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
message(STATUS "  Vendor SDL3: ${ASTRAL_VENDOR_SDL3}")
message(STATUS "  Link SDL3main: ${ASTRAL_LINK_SDL3MAIN}")
message(STATUS "  Warnings as Errors: ${ASTRAL_WARNINGS_AS_ERRORS}")
message(STATUS "  Profiling: ${ASTRAL_ENABLE_PROFILING}")
message(STATUS "  Vulkan Support: ${ASTRAL_ENABLE_VULKAN}")
message(STATUS "  Vulkan Validation: ${ASTRAL_VULKAN_VALIDATION}")
message(STATUS "  Vendor Vulkan: ${ASTRAL_VENDOR_VULKAN}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  SDL3: Found")
if(GLM_INCLUDE_DIR)
    message(STATUS "  GLM: Found at ${GLM_INCLUDE_DIR}")
elseif(TARGET glm::glm)
    message(STATUS "  GLM: System installation")
endif()
if(ASTRAL_HAS_VULKAN)
    message(STATUS "  Vulkan: Found (Version ${ASTRAL_VULKAN_VERSION})")
else()
    message(STATUS "  Vulkan: Not found (optional)")
endif()
message(STATUS "")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "==========================================")
