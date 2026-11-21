add_library(AstralRender STATIC
    src/Graph/RenderGraph.cpp
    src/MeshPrimitives.cpp
    src/Renderer.cpp
    src/RenderView.cpp
    src/SdfVolume.cpp
    src/World.cpp
    src/Vk/Device.cpp
    src/Vk/Pipelines.cpp
    src/Vk/Resources.cpp
    src/Vk/Swapchain.cpp
    src/Vk/VmaImplementation.cpp)
add_library(Astral::Render ALIAS AstralRender)

target_include_directories(AstralRender PUBLIC include PRIVATE src)
target_link_libraries(AstralRender PUBLIC Astral::Core Vulkan::Vulkan)
# VMA is header-only; include it as a system header so its warnings stay out of our build.
target_include_directories(AstralRender SYSTEM PRIVATE ${vma_SOURCE_DIR}/include)
set_source_files_properties(src/Vk/VmaImplementation.cpp PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang>:-w>")
target_compile_definitions(AstralRender PRIVATE
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1)
astral_configure_target(AstralRender)

file(GLOB ASTRAL_SHADER_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp)
file(GLOB ASTRAL_SHADER_INCLUDES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/*.glsl)
astral_embed_shaders(AstralRender SOURCES ${ASTRAL_SHADER_SOURCES} DEPENDS ${ASTRAL_SHADER_INCLUDES})
