# Astral::Script — Luau scripting (ADR-0017). Luau (MIT) is fetched and compiled into this library
# (VM, compiler, parser; no Luau headers or types in public headers).
include(FetchContent)
FetchContent_Declare(luau
    GIT_REPOSITORY https://github.com/luau-lang/luau.git
    GIT_TAG 0.748
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(luau)
if(NOT luau_POPULATED)
    FetchContent_Populate(luau)
endif()
file(GLOB luau_sources CONFIGURE_DEPENDS
    ${luau_SOURCE_DIR}/VM/src/*.cpp
    ${luau_SOURCE_DIR}/Compiler/src/*.cpp
    ${luau_SOURCE_DIR}/Bytecode/src/*.cpp
    ${luau_SOURCE_DIR}/Common/src/*.cpp
    ${luau_SOURCE_DIR}/Ast/src/*.cpp)

astral_add_module(NAME script
    SOURCES
        src/host.cpp
        src/bindings.cpp
        src/bindings_services.cpp
        src/convert.cpp
        src/api.cpp
        src/compile.cpp
        ${luau_sources}
    DEPS
        Astral::Core
        Astral::World)
set_source_files_properties(${luau_sources} PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")
target_include_directories(astral_script SYSTEM PRIVATE
    ${luau_SOURCE_DIR}/VM/include ${luau_SOURCE_DIR}/VM/src
    ${luau_SOURCE_DIR}/Compiler/include ${luau_SOURCE_DIR}/Bytecode/include ${luau_SOURCE_DIR}/Ast/include ${luau_SOURCE_DIR}/Common/include)

astral_add_tests(MODULE script
    SOURCES
        ${CMAKE_SOURCE_DIR}/modules/core/tests/test_main.cpp
        tests/script_test.cpp
        tests/api_test.cpp
        tests/api_parity_test.cpp
        tests/compile_test.cpp)
