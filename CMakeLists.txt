# /**********************************************************************
#  * Astral Engine - Professional AAA Game Engine CMake Configuration
#  * Purpose: Modern CMake configuration with vcpkg integration for
#  *          professional game development
#  * Author: Astral Engine Development Team
#  * Version: 0.2.0-alpha
#  * Date: 2025-09-19
#  * Signature: inkbytefo
#  **********************************************************************/

cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# Project configuration
project(AstralEngine
    VERSION 0.2.0
    DESCRIPTION "Professional AAA Game Engine with Modern C++20"
    LANGUAGES CXX C
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include CMake modules
include(FetchContent)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# =====================================================================
# VCPKG INTEGRATION (Primary Dependency Management)
# =====================================================================

# Detect vcpkg toolchain
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(STATUS "Using vcpkg toolchain: $ENV{VCPKG_ROOT}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(STATUS "Using local vcpkg toolchain")
else()
    message(WARNING "vcpkg not found. Please install vcpkg or set VCPKG_ROOT environment variable.")
    message(WARNING "See BUILD_INSTRUCTIONS.md for setup instructions.")
endif()

# =====================================================================
# BUILD CONFIGURATION OPTIONS
# =====================================================================

# Core build options
option(ASTRAL_BUILD_SHARED "Build AstralEngine as shared library" OFF)
option(ASTRAL_BUILD_EXAMPLES "Build example applications" ON)
option(ASTRAL_BUILD_TESTS "Build unit tests" OFF)
option(ASTRAL_BUILD_TOOLS "Build development tools" OFF)
option(ASTRAL_ENABLE_PROFILING "Enable profiling support" OFF)
option(ASTRAL_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(ASTRAL_ENABLE_LTO "Enable Link Time Optimization" OFF)

# Subsystem options
option(ASTRAL_USE_SDL3 "Enable SDL3 platform backend" ON)
option(ASTRAL_USE_VULKAN "Enable Vulkan graphics backend" ON)
option(ASTRAL_USE_IMGUI "Enable Dear ImGui UI system" ON)
option(ASTRAL_USE_JOLT_PHYSICS "Enable Jolt Physics system" ON)

# Vendor options (automatic download & build)
option(ASTRAL_VENDOR_SDL3 "Automatically download and build SDL3" OFF)
option(ASTRAL_VENDOR_GLM "Automatically download and build GLM" OFF)
option(ASTRAL_VENDOR_NLOHMANN_JSON "Automatically download and build nlohmann_json" OFF)
option(ASTRAL_VENDOR_ASSIMP "Automatically download and build assimp" OFF)
option(ASTRAL_VENDOR_VMA "Automatically download and build VulkanMemoryAllocator" OFF)
option(ASTRAL_VENDOR_IMGUI "Automatically download and build Dear ImGui" OFF)
option(ASTRAL_VENDOR_JOLT "Automatically download and build Jolt Physics" OFF)

# Development options
option(ASTRAL_ENABLE_VALIDATION "Enable Vulkan validation layers" OFF)
option(ASTRAL_ENABLE_DEBUG_MARKERS "Enable Vulkan debug markers" OFF)
option(ASTRAL_ENABLE_SHADER_HOT_RELOAD "Enable hot shader reloading" ON)

# Platform-specific options
if(WIN32)
    option(ASTRAL_LINK_SDL3MAIN "Link SDL3main on Windows" ON)
endif()

# =====================================================================
# DEPENDENCY MANAGEMENT
# =====================================================================

# Find or fetch dependencies based on options
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake")

# =====================================================================
# COMPILER AND PLATFORM CONFIGURATION
# =====================================================================

# Compiler-specific settings
if(MSVC)
    # MSVC specific settings
    add_compile_options(
        /W4                     # High warning level
        /permissive-           # Standards conformance
        /Zc:__cplusplus        # Enable correct __cplusplus macro value
        /Zc:preprocessor       # Enable standards-conforming preprocessor
        /Zc:lambda             # Enable lambda standards conformance
        /Zc:throwingNew        # Enable throwing new
    )

    # Enable AVX2 optimizations in Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/arch:AVX2)
    endif()

    # Enable Edit and Continue in Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/ZI)
    endif()

    # Link time optimization
    if(ASTRAL_ENABLE_LTO)
        add_compile_options(/GL)
        add_link_options(/LTCG)
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang settings
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wunused
        -Wuninitialized
        -Wundef
    )

    # Enable AVX2 optimizations in Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-march=native)
    endif()

    # Link time optimization
    if(ASTRAL_ENABLE_LTO)
        add_compile_options(-flto)
        add_link_options(-flto)
    endif()

    # Debug optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-Og -g3)
    else()
        add_compile_options(-O3)
    endif()
endif()

# Platform-specific definitions
if(WIN32)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        VK_USE_PLATFORM_WIN32_KHR
    )
elseif(APPLE)
    add_compile_definitions(VK_USE_PLATFORM_MACOS_MVK)
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(VK_USE_PLATFORM_XCB_KHR)
endif()

# Build type specific definitions
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(ASTRAL_DEBUG=1)
    if(ASTRAL_ENABLE_VALIDATION)
        add_compile_definitions(ASTRAL_VULKAN_VALIDATION=1)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(ASTRAL_RELEASE=1 NDEBUG)
endif()

# =====================================================================
# ASTRAL ENGINE LIBRARY TARGET
# =====================================================================

# Collect source files
file(GLOB_RECURSE ASTRAL_SOURCES
    "Source/*.cpp"
    "Source/*.h"
    "Source/**/*.cpp"
    "Source/**/*.h"
)

# Create library target
if(ASTRAL_BUILD_SHARED)
    add_library(AstralEngine SHARED ${ASTRAL_SOURCES})
    target_compile_definitions(AstralEngine PRIVATE ASTRAL_BUILD_SHARED)
else()
    add_library(AstralEngine STATIC ${ASTRAL_SOURCES})
    target_compile_definitions(AstralEngine PRIVATE ASTRAL_BUILD_STATIC)
endif()

# Set target properties
set_target_properties(AstralEngine PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "AstralEngine"
    DEBUG_POSTFIX "_d"
    RELWITHDEBINFO_POSTFIX "_rd"
)

# Include directories
target_include_directories(AstralEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link dependencies
target_link_libraries(AstralEngine PUBLIC
    # Core dependencies
    SDL3::SDL3
    Vulkan::Vulkan
    GLM::GLM
    nlohmann_json::nlohmann_json
    $<$<BOOL:${ASTRAL_USE_ASSIMP}>:assimp::assimp>
    VMA::VMA

    # Optional dependencies
    $<$<BOOL:${ASTRAL_USE_IMGUI}>:ImGui>
    $<$<BOOL:${ASTRAL_USE_JOLT_PHYSICS}>:Jolt>
)

# Compile definitions
target_compile_definitions(AstralEngine PUBLIC
    # Version info
    ASTRAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ASTRAL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ASTRAL_VERSION_PATCH=${PROJECT_VERSION_PATCH}

    # Feature flags
    $<$<BOOL:${ASTRAL_USE_SDL3}>:ASTRAL_USE_SDL3>
    $<$<BOOL:${ASTRAL_USE_VULKAN}>:ASTRAL_USE_VULKAN>
    $<$<BOOL:${ASTRAL_USE_IMGUI}>:ASTRAL_USE_IMGUI>
    $<$<BOOL:${ASTRAL_USE_JOLT_PHYSICS}>:ASTRAL_USE_JOLT_PHYSICS>

    # Development flags
    $<$<BOOL:${ASTRAL_ENABLE_VALIDATION}>:ASTRAL_VULKAN_VALIDATION>
    $<$<BOOL:${ASTRAL_ENABLE_DEBUG_MARKERS}>:ASTRAL_DEBUG_MARKERS>
    $<$<BOOL:${ASTRAL_ENABLE_SHADER_HOT_RELOAD}>:ASTRAL_SHADER_HOT_RELOAD>
)

# =====================================================================
# SUBSYSTEM CONFIGURATION
# =====================================================================

# Configure Astral target function
function(configure_astral_target target)
    # Include directories
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )

    # Link to AstralEngine
    target_link_libraries(${target} PRIVATE AstralEngine)

    # Compile definitions
    target_compile_definitions(${target} PRIVATE
        ASTRAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        ASTRAL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        ASTRAL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    )

    # Set C++ standard
    target_compile_features(${target} PRIVATE cxx_std_20)

    # IDE properties
    set_target_properties(${target} PROPERTIES
        DEBUG_POSTFIX "_d"
        RELWITHDEBINFO_POSTFIX "_rd"
    )

    # Copy assets to output directory
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target}>/Assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Assets
            $<TARGET_FILE_DIR:${target}>/Assets
        COMMENT "Copying assets to ${target} output directory"
    )

    # Copy compiled shaders if Vulkan is enabled
    if(ASTRAL_USE_VULKAN)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target}>/Assets/Shaders
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_BINARY_DIR}/shaders
                $<TARGET_FILE_DIR:${target}>/Assets/Shaders
            COMMENT "Copying compiled shaders to ${target} output directory"
        )
    endif()
endfunction()

# =====================================================================
# EXAMPLES
# =====================================================================

if(ASTRAL_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif()

# =====================================================================
# TESTS
# =====================================================================

if(ASTRAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# =====================================================================
# TOOLS
# =====================================================================

if(ASTRAL_BUILD_TOOLS)
    add_subdirectory(Tools)
endif()

# =====================================================================
# INSTALLATION
# =====================================================================

# Install library
install(TARGETS AstralEngine
    EXPORT AstralEngineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY Source/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/AstralEngine
    FILES_MATCHING PATTERN "*.h"
)

# Install CMake config files
install(EXPORT AstralEngineTargets
    FILE AstralEngineTargets.cmake
    NAMESPACE AstralEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralEngine
)

# Generate and install package config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AstralEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AstralEngineConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralEngine
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AstralEngineConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralEngine
)

# =====================================================================
# PACKAGING
# =====================================================================

# CPack configuration
set(CPACK_PACKAGE_NAME "AstralEngine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "Professional AAA Game Engine")
set(CPACK_PACKAGE_VENDOR "Astral Engine Development Team")
set(CPACK_PACKAGE_CONTACT "contact@astralengine.com")

# Platform-specific packaging
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "AstralEngine ${PROJECT_VERSION}")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "AstralEngine ${PROJECT_VERSION}")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)

# =====================================================================
# SUMMARY
# =====================================================================

message(STATUS "=== Astral Engine Configuration Summary ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Shared: ${ASTRAL_BUILD_SHARED}")
message(STATUS "Build Examples: ${ASTRAL_BUILD_EXAMPLES}")
message(STATUS "Build Tests: ${ASTRAL_BUILD_TESTS}")
message(STATUS "Build Tools: ${ASTRAL_BUILD_TOOLS}")
message(STATUS "Enable Profiling: ${ASTRAL_ENABLE_PROFILING}")
message(STATUS "Enable LTO: ${ASTRAL_ENABLE_LTO}")
message(STATUS "Warnings as Errors: ${ASTRAL_WARNINGS_AS_ERRORS}")
message(STATUS "")
message(STATUS "Subsystems:")
message(STATUS "  SDL3: ${ASTRAL_USE_SDL3}")
message(STATUS "  Vulkan: ${ASTRAL_USE_VULKAN}")
message(STATUS "  ImGui: ${ASTRAL_USE_IMGUI}")
message(STATUS "  Jolt Physics: ${ASTRAL_USE_JOLT_PHYSICS}")
message(STATUS "")
message(STATUS "Development Features:")
message(STATUS "  Validation: ${ASTRAL_ENABLE_VALIDATION}")
message(STATUS "  Debug Markers: ${ASTRAL_ENABLE_DEBUG_MARKERS}")
message(STATUS "  Shader Hot Reload: ${ASTRAL_ENABLE_SHADER_HOT_RELOAD}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  vcpkg Toolchain: ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "  SDL3: ${SDL3_FOUND}")
message(STATUS "  Vulkan: ${Vulkan_FOUND}")
message(STATUS "  GLM: ${GLM_FOUND}")
message(STATUS "  ImGui: ${IMGUI_FOUND}")
message(STATUS "  Jolt Physics: ${JOLT_FOUND}")
message(STATUS "============================================")
