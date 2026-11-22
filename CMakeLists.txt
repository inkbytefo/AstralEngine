cmake_minimum_required(VERSION 3.24)
project(Astral VERSION 0.17.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASTRAL_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(ASTRAL_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(ASTRAL_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(ASTRAL_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(ASTRAL_SIZE_BUDGET_STRICT "Fail code_size_check when unexcepted offenders exceed the M13.5 size budget" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AstralWarnings)
include(AstralModule)
include(AstralShaders)
include(AstralEmbed)
include(GNUInstallDirs)

if(ASTRAL_BUILD_TESTS)
    enable_testing()
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    # doctest's forward declaration of std::basic_ostream breaks with the MSVC 14.51 STL.
    set(DOCTEST_USE_STD_HEADERS ON CACHE BOOL "" FORCE)
    include(FetchContent)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.5.3
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(doctest)
endif()

# Modules — order follows the dependency layers (lower first).
add_subdirectory(modules/core)
add_subdirectory(modules/shader)
add_subdirectory(modules/gpu)
add_subdirectory(modules/platform)
add_subdirectory(modules/world)
add_subdirectory(modules/script)
add_subdirectory(modules/physics)
add_subdirectory(modules/render)
add_subdirectory(modules/audio)
add_subdirectory(modules/assets)
add_subdirectory(modules/app)
set(ASTRAL_MODULE_GRAPH_FILE "${CMAKE_CURRENT_BINARY_DIR}/astral_module_graph.json")
astral_export_module_graph("${ASTRAL_MODULE_GRAPH_FILE}")
add_subdirectory(tools)
include(AstralGame)

if(ASTRAL_BUILD_TESTS)
    add_subdirectory(tests/integration)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        # The shared features (features/) and their copies inside the games must be identical.
        add_test(NAME shared_features_in_sync COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/sync_features.py --check)
        set(_size_check_args --root ${CMAKE_SOURCE_DIR})
        if(ASTRAL_SIZE_BUDGET_STRICT)
            list(APPEND _size_check_args --strict)
        endif()
        add_test(NAME code_size_check
            COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/check_sizes.py ${_size_check_args})
        add_test(NAME layer_check
            COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.py"
                    --manifest "${ASTRAL_MODULE_GRAPH_FILE}"
                    --allowlist "${CMAKE_CURRENT_SOURCE_DIR}/tools/module_layers.json")
        add_test(NAME layer_check_self_test
            COMMAND Python3::Interpreter -B -m unittest discover
                    -s "${CMAKE_CURRENT_SOURCE_DIR}/tools/tests" -p "test_check_layers.py" -v)
    else()
        message(STATUS "Python3 interpreter not found: skipping shared_features_in_sync, code_size_check, and layer_check")
    endif()
    add_test(NAME astral_public_headers_clean
        COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/CheckPublicHeaders.cmake)
endif()

if(ASTRAL_BUILD_EXAMPLES)
    add_subdirectory(templates/blank)
    add_subdirectory(examples)
    add_subdirectory(bench)
    if(ASTRAL_BUILD_TESTS)
        add_subdirectory(tests/golden)
    endif()
endif()

if(ASTRAL_INSTALL)
    include(AstralInstall)
endif()
