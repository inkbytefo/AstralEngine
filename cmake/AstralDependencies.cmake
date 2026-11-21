# Third-party dependencies, fetched at configure time and pinned to exact versions.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

find_package(Vulkan 1.3 REQUIRED)

# --- Header-only / small -------------------------------------------------------
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1 GIT_SHALLOW TRUE)

FetchContent_Declare(vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.1.0 GIT_SHALLOW TRUE)

FetchContent_Declare(cgltf GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git GIT_TAG v1.14 GIT_SHALLOW TRUE)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 5c205738c191bcb0abc65c4febfa9bd25ff35234)

FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3 GIT_SHALLOW TRUE)

# --- Libraries ------------------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
    find_program(ASTRAL_WAYLAND_SCANNER wayland-scanner)
    if(NOT ASTRAL_WAYLAND_SCANNER)
        message(STATUS "wayland-scanner not found: building GLFW with X11 only")
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    endif()
endif()
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)

set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.0.0 GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build)

FetchContent_MakeAvailable(glm vma cgltf stb json glfw JoltPhysics)

if(TARGET Jolt AND NOT MSVC)
    target_compile_options(Jolt PRIVATE -w)
endif()

# Treat third-party headers as system headers so their warnings do not drown ours.
foreach(dep Jolt glm glfw)
    if(TARGET ${dep})
        get_target_property(dep_includes ${dep} INTERFACE_INCLUDE_DIRECTORIES)
        if(dep_includes)
            set_target_properties(${dep} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${dep_includes}")
        endif()
    endif()
endforeach()

add_library(astral_cgltf INTERFACE)
target_include_directories(astral_cgltf SYSTEM INTERFACE ${cgltf_SOURCE_DIR})
add_library(astral_stb INTERFACE)
target_include_directories(astral_stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

# --- Editor-only ----------------------------------------------------------------
if(ASTRAL_BUILD_EDITOR)
    FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9b-docking GIT_SHALLOW TRUE)
    # A commit hash cannot be shallow-cloned, so ImGuizmo is fetched in full.
    FetchContent_Declare(imguizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG 18cef5e031d8c6973d80284c67f60549fafd78c1)
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()
    FetchContent_GetProperties(imguizmo)
    if(NOT imguizmo_POPULATED)
        FetchContent_Populate(imguizmo)
    endif()

    add_library(astral_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
    target_include_directories(astral_imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imguizmo_SOURCE_DIR}/src)
    target_compile_definitions(astral_imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
    target_link_libraries(astral_imgui PUBLIC Vulkan::Headers glfw)
endif()

if(ASTRAL_BUILD_TESTS)
    FetchContent_Declare(doctest GIT_REPOSITORY https://github.com/doctest/doctest.git GIT_TAG v2.4.11 GIT_SHALLOW TRUE)
    FetchContent_GetProperties(doctest)
    if(NOT doctest_POPULATED)
        FetchContent_Populate(doctest)
    endif()
    add_library(astral_doctest INTERFACE)
    target_include_directories(astral_doctest SYSTEM INTERFACE ${doctest_SOURCE_DIR})
endif()
