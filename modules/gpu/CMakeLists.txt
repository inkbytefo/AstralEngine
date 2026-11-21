find_package(Vulkan 1.3 REQUIRED)

include(FetchContent)
FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.1.0
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(VulkanMemoryAllocator)
if(NOT vulkanmemoryallocator_POPULATED)
    FetchContent_Populate(VulkanMemoryAllocator) # header-only: no need for its CMake project
endif()

astral_add_module(NAME gpu
    SOURCES
        src/device.cpp
        src/resources.cpp
        src/pipelines.cpp
        src/command_list.cpp
        src/swapchain.cpp
        src/vma.cpp
    DEPS
        Astral::Core
    PRIVATE_DEPS
        Vulkan::Vulkan)
target_include_directories(astral_gpu SYSTEM PRIVATE
    $<BUILD_INTERFACE:${vulkanmemoryallocator_SOURCE_DIR}/include>)

if(ASTRAL_INSTALL)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/ DESTINATION ${CMAKE_INSTALL_DATADIR}/astral/shaders)
endif()

astral_add_tests(MODULE gpu
    SOURCES
        tests/test_main.cpp
        tests/device_test.cpp
        tests/resource_test.cpp
        tests/command_test.cpp
        tests/robustness_test.cpp)
if(TARGET astral_gpu_tests)
    astral_add_shaders(astral_gpu_tests SOURCES
        tests/shaders/fill.comp
        tests/shaders/triangle.vert
        tests/shaders/solid.frag
        tests/shaders/write_image.comp
        tests/shaders/read_depth.comp)
endif()

# `VkFooInfo info{VK_STRUCTURE_TYPE_FOO}` (zero-initialise the rest) is the idiomatic Vulkan
# pattern; -Wextra would flag every one of them.
if(NOT MSVC)
    target_compile_options(astral_gpu PRIVATE -Wno-missing-field-initializers)
endif()
