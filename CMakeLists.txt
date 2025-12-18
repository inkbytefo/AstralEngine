# /**********************************************************************
#  * Astral Engine - Professional AAA Game Engine CMake Configuration
#  * Purpose: Modern, modular, and target-centric CMake configuration.
#  * Author: Astral Engine Development Team
#  * Version: 0.2.0-alpha
#  * Date: 2025-09-19
#  **********************************************************************/

cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# Compatibility policy for older dependency projects
if(POLICY CMP0000)
    cmake_policy(SET CMP0000 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.5)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# --- Project Definition ---
project(AstralEngine
    VERSION 0.2.0
    DESCRIPTION "Professional AAA Game Engine with Modern C++20"
    LANGUAGES CXX C
)

# --- Core CMake Settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Include Helper Modules ---
include(FetchContent)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(cmake/ProjectOptions.cmake) # Include our custom build options

# --- VCPKG Integration ---
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
else()
    message(WARNING "vcpkg not found. Dependencies will be handled by FetchContent if enabled.")
endif()

# --- Dependency Management ---
include(cmake/Dependencies.cmake) # Handles finding or fetching all dependencies

# --- Project-Wide Configuration ---
include(cmake/ProjectConfiguration.cmake) # Defines the 'astral_common_settings' INTERFACE target

# =====================================================================
# ASTRAL ENGINE LIBRARY TARGET
# =====================================================================

# Explicitly list all source files for better build system reliability
set(ASTRAL_SOURCES
    # Core
    Source/Core/AstralImConfig.h
    Source/Core/Engine.cpp
    Source/Core/Engine.h
    Source/Core/FileLogger.cpp
    Source/Core/FileLogger.h
    Source/Core/IApplication.h
    Source/Core/ISubsystem.h
    Source/Core/Logger.cpp
    Source/Core/Logger.h
    Source/Core/MemoryManager.cpp
    Source/Core/MemoryManager.h
    Source/Core/ThreadPool.cpp
    Source/Core/ThreadPool.h
    Source/Core/Math/Vector2.h
    Source/Core/Math/Vector4.h

    # Events
    Source/Events/ApplicationEvent.h
    Source/Events/Event.h
    Source/Events/EventManager.cpp
    Source/Events/EventManager.h
    Source/Events/KeyEvent.h
    Source/Events/MouseEvent.h

    # ECS
    Source/ECS/Components.h
    Source/Subsystems/ECS/ECSSubsystem.cpp
    Source/Subsystems/ECS/ECSSubsystem.h

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
    Source/Subsystems/Asset/IAssetImporter.h
    Source/Subsystems/Asset/MaterialImporter.cpp
    Source/Subsystems/Asset/MaterialImporter.h
    Source/Subsystems/Asset/Model.cpp
    Source/Subsystems/Asset/Model.h
    Source/Subsystems/Asset/ModelImporter.cpp
    Source/Subsystems/Asset/ModelImporter.h
    Source/Subsystems/Asset/ShaderImporter.cpp
    Source/Subsystems/Asset/ShaderImporter.h
    Source/Subsystems/Asset/ShaderProgram.h
    Source/Subsystems/Asset/TextureImporter.cpp
    Source/Subsystems/Asset/TextureImporter.h

    # Platform
    Source/Subsystems/Platform/InputManager.cpp
    Source/Subsystems/Platform/InputManager.h
    Source/Subsystems/Platform/KeyCode.h
    Source/Subsystems/Platform/PlatformSubsystem.cpp
    Source/Subsystems/Platform/PlatformSubsystem.h
    Source/Subsystems/Platform/Window.cpp
    Source/Subsystems/Platform/Window.h

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
    Source/Subsystems/Renderer/PostProcessingEffectBase.cpp
    Source/Subsystems/Renderer/PostProcessingEffectBase.h
    Source/Subsystems/Renderer/PostProcessingSubsystem.cpp
    Source/Subsystems/Renderer/PostProcessingSubsystem.h
    Source/Subsystems/Renderer/RendererTypes.h
    Source/Subsystems/Renderer/RenderSubsystem.cpp
    Source/Subsystems/Renderer/RenderSubsystem.h
    Source/Subsystems/Renderer/TonemappingEffect.cpp
    Source/Subsystems/Renderer/TonemappingEffect.h

    # Third Party
    Source/ThirdParty/stb_image.cpp
    Source/ThirdParty/stb_image.h

    # Main
    Source/main.cpp
    Source/test_sdl3.cpp
)

if(ASTRAL_BUILD_SHARED)
    add_library(AstralEngine SHARED ${ASTRAL_SOURCES})
    target_compile_definitions(AstralEngine PRIVATE ASTRAL_BUILD_SHARED)
else()
    add_library(AstralEngine STATIC ${ASTRAL_SOURCES})
    target_compile_definitions(AstralEngine PRIVATE ASTRAL_BUILD_STATIC)
endif()

set_target_properties(AstralEngine PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "AstralEngine"
    DEBUG_POSTFIX "_d"
    RELWITHDEBINFO_POSTFIX "_rd"
)

# --- Include Directories ---
target_include_directories(AstralEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# --- Link Dependencies and Common Settings ---
target_link_libraries(AstralEngine PUBLIC
    # Link our common settings INTERFACE target
    astral_common_settings

    # Core dependencies
    SDL3::SDL3
    Vulkan::Vulkan
    glm # GLM uses 'glm' target name, not 'GLM::GLM'
    nlohmann_json::nlohmann_json
    assimp::assimp
    VMA

    # Optional dependencies
    $<$<BOOL:${ASTRAL_USE_IMGUI}>:ImGui>
    $<$<BOOL:${ASTRAL_USE_JOLT_PHYSICS}>:Jolt>
)

# --- Add DevTools Subsystem ---
add_subdirectory(Source/Subsystems/DevTools)

# =====================================================================
# SUB-PROJECTS (Examples, Tests, Tools)
# =====================================================================

# This function configures any executable target (like examples or tools)
# to correctly link against the engine and inherit its settings.
function(configure_astral_target target)
    target_link_libraries(${target} PRIVATE AstralEngine)

    set_target_properties(${target} PROPERTIES
        DEBUG_POSTFIX "_d"
        RELWITHDEBINFO_POSTFIX "_rd"
    )
    
    # Copy assets to output directory
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:${target}>/Assets
        COMMENT "Copying assets to ${target} output directory"
    )

    # Copy compiled shaders if Vulkan is enabled
    if(ASTRAL_USE_VULKAN)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_BINARY_DIR}/shaders $<TARGET_FILE_DIR:${target}>/Assets/Shaders
            COMMENT "Copying compiled shaders to ${target} output directory"
        )
    endif()
endfunction()

if(ASTRAL_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif()

if(ASTRAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(ASTRAL_BUILD_TOOLS)
    add_subdirectory(Tools)
endif()

# =====================================================================
# INSTALLATION & PACKAGING
# =====================================================================

# --- Install Configuration ---
include(CMakePackageConfigHelpers)

# Generate the config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AstralEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AstralEngineConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralEngine
)

# Generate the version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AstralEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)


# Create a list of optional dependency targets
set(ASTRAL_DEPENDENCY_TARGETS
    SDL3_Headers
    SDL3-shared
    glm-header-only
    glm
    VulkanMemoryAllocator
    VMA
    nlohmann_json
    assimp
    ImGui
    Jolt
)

# Install all available dependency targets in a single command
foreach(dep IN LISTS ASTRAL_DEPENDENCY_TARGETS)
    if(TARGET ${dep})
        list(APPEND EXPORT_TARGETS ${dep})
    endif()
endforeach()

# Install the AstralEngine target along with all dependencies in a single export set
install(TARGETS AstralEngine astral_common_settings ${EXPORT_TARGETS}
    EXPORT AstralEngineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the header files
install(DIRECTORY Source/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Install the config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/AstralEngineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AstralEngineConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralEngine
)

# Install the export targets
install(EXPORT AstralEngineTargets
    FILE AstralEngineTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralEngine
)

# =====================================================================
# PACKAGING (CPack)
# =====================================================================

set(CPACK_PACKAGE_NAME "AstralEngine")
set(CPACK_PACKAGE_VENDOR "Astral Engine Development Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Professional AAA Game Engine with Modern C++20")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)

# =====================================================================
# SUMMARY
# =====================================================================

message(STATUS "====================================================================")
message(STATUS "AstralEngine Build Configuration Summary")
message(STATUS "====================================================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "====================================================================")
message(STATUS "Build Options:")
message(STATUS "  Build Shared Library: ${ASTRAL_BUILD_SHARED}")
message(STATUS "  Build Examples: ${ASTRAL_BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${ASTRAL_BUILD_TESTS}")
message(STATUS "  Build Tools: ${ASTRAL_BUILD_TOOLS}")
message(STATUS "  Enable Profiling: ${ASTRAL_ENABLE_PROFILING}")
message(STATUS "  Warnings as Errors: ${ASTRAL_WARNINGS_AS_ERRORS}")
message(STATUS "  Enable LTO: ${ASTRAL_ENABLE_LTO}")
message(STATUS "====================================================================")
message(STATUS "Subsystem Options:")
message(STATUS "  Use SDL3: ${ASTRAL_USE_SDL3}")
message(STATUS "  Use Vulkan: ${ASTRAL_USE_VULKAN}")
message(STATUS "  Use ImGui: ${ASTRAL_USE_IMGUI}")
message(STATUS "  Use Jolt Physics: ${ASTRAL_USE_JOLT_PHYSICS}")
message(STATUS "====================================================================")
message(STATUS "Development Options:")
message(STATUS "  Enable Validation: ${ASTRAL_ENABLE_VALIDATION}")
message(STATUS "  Enable Debug Markers: ${ASTRAL_ENABLE_DEBUG_MARKERS}")
message(STATUS "  Enable Shader Hot Reload: ${ASTRAL_ENABLE_SHADER_HOT_RELOAD}")
message(STATUS "====================================================================")
