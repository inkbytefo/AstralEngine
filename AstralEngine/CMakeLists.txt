# Motor cekirdegi — STATIC kutuphane. ImGui / ImGuizmo / Editor bu hedefe girmez.

find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(SHADER_SRC "${CMAKE_SOURCE_DIR}/shaders/SDFCompute.glsl")
set(SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/SDFCompute.spv")

add_custom_command(
    OUTPUT ${SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=compute ${SHADER_SRC} -o ${SHADER_SPV}
    DEPENDS ${SHADER_SRC}
    COMMENT "Compiling ${SHADER_SRC} -> ${SHADER_SPV}"
)

set(TAA_SHADER_SRC "${CMAKE_SOURCE_DIR}/shaders/TAAResolve.glsl")
set(TAA_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/TAAResolve.spv")

add_custom_command(
    OUTPUT ${TAA_SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=compute ${TAA_SHADER_SRC} -o ${TAA_SHADER_SPV}
    DEPENDS ${TAA_SHADER_SRC}
    COMMENT "Compiling ${TAA_SHADER_SRC} -> ${TAA_SHADER_SPV}"
)

set(GBUFFER_SHADER_SRC "${CMAKE_SOURCE_DIR}/shaders/SDFGBuffer.glsl")
set(GBUFFER_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/SDFGBuffer.spv")

add_custom_command(
    OUTPUT ${GBUFFER_SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=compute ${GBUFFER_SHADER_SRC} -o ${GBUFFER_SHADER_SPV}
    DEPENDS ${GBUFFER_SHADER_SRC}
    COMMENT "Compiling ${GBUFFER_SHADER_SRC} -> ${GBUFFER_SHADER_SPV}"
)

set(DEBUG_COMPOSITE_SHADER_SRC "${CMAKE_SOURCE_DIR}/shaders/SDFDebugComposite.glsl")
set(DEBUG_COMPOSITE_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/SDFDebugComposite.spv")

add_custom_command(
    OUTPUT ${DEBUG_COMPOSITE_SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=compute ${DEBUG_COMPOSITE_SHADER_SRC} -o ${DEBUG_COMPOSITE_SHADER_SPV}
    DEPENDS ${DEBUG_COMPOSITE_SHADER_SRC}
    COMMENT "Compiling ${DEBUG_COMPOSITE_SHADER_SRC} -> ${DEBUG_COMPOSITE_SHADER_SPV}"
)

set(DEFERRED_LIGHTING_SHADER_SRC "${CMAKE_SOURCE_DIR}/shaders/DeferredLighting.glsl")
set(DEFERRED_LIGHTING_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/DeferredLighting.spv")

add_custom_command(
    OUTPUT ${DEFERRED_LIGHTING_SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=compute ${DEFERRED_LIGHTING_SHADER_SRC} -o ${DEFERRED_LIGHTING_SHADER_SPV}
    DEPENDS ${DEFERRED_LIGHTING_SHADER_SRC}
    COMMENT "Compiling ${DEFERRED_LIGHTING_SHADER_SRC} -> ${DEFERRED_LIGHTING_SHADER_SPV}"
)

add_custom_target(CompileShaders ALL DEPENDS 
    ${SHADER_SPV} 
    ${TAA_SHADER_SPV} 
    ${GBUFFER_SHADER_SPV} 
    ${DEBUG_COMPOSITE_SHADER_SPV}
    ${DEFERRED_LIGHTING_SHADER_SPV}
)

add_library(AstralEngine STATIC
    ${CMAKE_SOURCE_DIR}/src/Core/Application.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Window.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/InputSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Systems/InputSubsystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Systems/PhysicsSubsystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Systems/TransformSubsystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Systems/RenderExtractionSubsystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/TransformSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/BenchmarkLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/RenderExtractionSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/VulkanContext.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/VmaImplementation.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/ComputePipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/Buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/BrickGrid.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/IBLManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/SDFRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/Swapchain.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/CommandStack.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Input/ActionMap.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Threading/JobSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Threading/TaskGraph.cpp
    ${CMAKE_SOURCE_DIR}/src/Scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/src/Scene/SceneSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/Scene/SDFWorldQuery.cpp
    ${CMAKE_SOURCE_DIR}/src/Project/Project.cpp
    ${CMAKE_SOURCE_DIR}/src/Project/ProjectSerializer.cpp
)

add_dependencies(AstralEngine CompileShaders)

target_include_directories(AstralEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(AstralEngine PUBLIC
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    SHADER_BIN_DIR="${CMAKE_BINARY_DIR}/shaders"
)

target_link_libraries(AstralEngine PUBLIC
    Vulkan::Vulkan
    VulkanMemoryAllocator
    glfw
    glm::glm
)

set_target_properties(AstralEngine PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)



get_target_property(_astral_link AstralEngine LINK_LIBRARIES)
get_target_property(_astral_iface_link AstralEngine INTERFACE_LINK_LIBRARIES)
get_target_property(_astral_inc AstralEngine INCLUDE_DIRECTORIES)
get_target_property(_astral_iface_inc AstralEngine INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "AstralEngine LINK_LIBRARIES=${_astral_link}")
message(STATUS "AstralEngine INTERFACE_LINK_LIBRARIES=${_astral_iface_link}")
message(STATUS "AstralEngine INCLUDE_DIRECTORIES=${_astral_inc}")
message(STATUS "AstralEngine INTERFACE_INCLUDE_DIRECTORIES=${_astral_iface_inc}")
