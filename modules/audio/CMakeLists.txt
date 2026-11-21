include(FetchContent)
enable_language(C)
# miniaudio (public domain / MIT-0): device output, decoding (WAV, FLAC, MP3), mixing, 3D panning.
# Compiled into this module; backends are loaded at run time (no system audio libraries to link).
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.26
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()
set_source_files_properties(${miniaudio_SOURCE_DIR}/miniaudio.c PROPERTIES COMPILE_OPTIONS "$<IF:$<C_COMPILER_ID:MSVC>,/W0,-w>" SKIP_LINTING ON)

find_package(Threads REQUIRED)
astral_add_module(NAME audio
    SOURCES
        src/audio.cpp
        ${miniaudio_SOURCE_DIR}/miniaudio.c
    DEPS
        Astral::Core
    PRIVATE_DEPS
        Threads::Threads
        ${CMAKE_DL_LIBS})
target_include_directories(astral_audio SYSTEM PRIVATE $<BUILD_INTERFACE:${miniaudio_SOURCE_DIR}>)
target_compile_definitions(astral_audio PRIVATE MA_NO_ENCODING MA_NO_GENERATION)
if(UNIX AND NOT APPLE)
    target_link_libraries(astral_audio PRIVATE m)
endif()

astral_add_tests(MODULE audio
    SOURCES
        tests/test_main.cpp
        tests/audio_test.cpp)
