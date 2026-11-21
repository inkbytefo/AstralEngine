include(FetchContent)
# stb_image (public domain / MIT): image decoding only, private to this module.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 6b5799bae8a1fe3792691ef4b72ae26434c3e799)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb) # header-only: no CMake project
endif()

astral_add_module(NAME assets
    SOURCES
        src/image.cpp
        src/gltf.cpp
        src/asset_manager.cpp
    DEPS
        Astral::Core
        Astral::Render)
target_include_directories(astral_assets SYSTEM PRIVATE $<BUILD_INTERFACE:${stb_SOURCE_DIR}>)

astral_add_tests(MODULE assets
    SOURCES
        tests/test_main.cpp
        tests/gltf_test.cpp
        tests/asset_manager_test.cpp)
if(TARGET astral_assets_tests)
    target_include_directories(astral_assets_tests SYSTEM PRIVATE ${stb_SOURCE_DIR})
endif()
