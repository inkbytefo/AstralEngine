include(FetchContent)
enable_language(C) # zstd and bc7enc are C

# stb_image (public domain / MIT): image decoding only, private to this module.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 6b5799bae8a1fe3792691ef4b72ae26434c3e799)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb) # header-only: no CMake project
endif()

# bc7enc + rgbcx (MIT / public domain): BC7 and BC5 encoding and decoding for the cook step.
FetchContent_Declare(bc7enc
    GIT_REPOSITORY https://github.com/richgel999/bc7enc.git
    GIT_TAG f66c2e489b07138f2673a2fb3d27c1aa1d565c48)
FetchContent_GetProperties(bc7enc)
if(NOT bc7enc_POPULATED)
    FetchContent_Populate(bc7enc) # we compile the three files we need ourselves
endif()

# zstd (BSD): .apak block compression. Compiled into this module (no separate library to install).
FetchContent_Declare(zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.7
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(zstd)
if(NOT zstd_POPULATED)
    FetchContent_Populate(zstd)
endif()
file(GLOB zstd_sources
    ${zstd_SOURCE_DIR}/lib/common/*.c
    ${zstd_SOURCE_DIR}/lib/compress/*.c
    ${zstd_SOURCE_DIR}/lib/decompress/*.c)
set(third_party_sources ${zstd_sources} ${bc7enc_SOURCE_DIR}/bc7enc.c ${bc7enc_SOURCE_DIR}/bc7decomp.cpp)
set_source_files_properties(${third_party_sources} PROPERTIES COMPILE_OPTIONS "-w" SKIP_LINTING ON)
if(MSVC)
    set_source_files_properties(${third_party_sources} PROPERTIES COMPILE_OPTIONS "/W0")
endif()

astral_add_module(NAME assets
    SOURCES
        src/image.cpp
        src/gltf.cpp
        src/asset_manager.cpp
        src/texture_codec.cpp
        src/cooked_model.cpp
        src/cook.cpp
        src/pack.cpp
        ${third_party_sources}
    DEPS
        Astral::Core
        Astral::Render)
target_include_directories(astral_assets SYSTEM PRIVATE
    $<BUILD_INTERFACE:${stb_SOURCE_DIR}>
    $<BUILD_INTERFACE:${bc7enc_SOURCE_DIR}>
    $<BUILD_INTERFACE:${zstd_SOURCE_DIR}/lib>)
target_compile_definitions(astral_assets PRIVATE ZSTD_DISABLE_ASM=1 ZSTD_LEGACY_SUPPORT=0)

astral_add_tests(MODULE assets
    SOURCES
        tests/test_main.cpp
        tests/gltf_test.cpp
        tests/asset_manager_test.cpp
        tests/cook_test.cpp)
if(TARGET astral_assets_tests)
    target_include_directories(astral_assets_tests SYSTEM PRIVATE ${stb_SOURCE_DIR})
endif()
