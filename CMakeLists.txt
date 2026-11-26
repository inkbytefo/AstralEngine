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

# Ninja: the heaviest translation units (Jolt) compile in a capped pool so a fully parallel build
# does not exhaust compiler heap on low-memory machines (MSVC C1060). Scheduling only, same output.
if(CMAKE_GENERATOR MATCHES "Ninja")
    cmake_host_system_information(RESULT _astral_cores QUERY NUMBER_OF_LOGICAL_CORES)
    math(EXPR _astral_heavy_jobs "${_astral_cores} / 2")
    if(_astral_heavy_jobs LESS 1)
        set(_astral_heavy_jobs 1)
    endif()
    set(ASTRAL_HEAVY_COMPILE_JOBS ${_astral_heavy_jobs} CACHE STRING
        "Ninja: parallel compile jobs for heavy translation units (Jolt)")
    if(NOT ASTRAL_HEAVY_COMPILE_JOBS MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "ASTRAL_HEAVY_COMPILE_JOBS must be a positive integer, got '${ASTRAL_HEAVY_COMPILE_JOBS}'")
    endif()
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS heavy_compile=${ASTRAL_HEAVY_COMPILE_JOBS})
    set(ASTRAL_HEAVY_COMPILE_POOL heavy_compile)
endif()

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
add_subdirectory(modules/editor_core)
add_subdirectory(modules/editor)
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
        add_test(NAME editor_isolation_check
            COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_editor_isolation.py"
                    --manifest "${ASTRAL_MODULE_GRAPH_FILE}")
    else()
        message(STATUS "Python3 interpreter not found: skipping shared_features_in_sync, code_size_check, and layer_check")
    endif()
    if(ASTRAL_HEAVY_COMPILE_POOL)
        add_test(NAME heavy_compile_pool_check
            COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/CheckJobPools.cmake)
    endif()
    add_test(NAME astral_public_headers_clean
        COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/CheckPublicHeaders.cmake)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
        add_test(NAME test_environment_check
            COMMAND ${CMAKE_COMMAND} -DCTEST=${CMAKE_CTEST_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
                    -P ${CMAKE_SOURCE_DIR}/cmake/CheckTestEnvironment.cmake)
    endif()
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

# Tests keep their SPIR-V cache in the build folder, never in the user's profile (the App's default).
# Runs last so it reaches every test registered above, in every subdirectory. Needs CMake 3.28
# (set_property(TEST ... DIRECTORY)); older CMake keeps the App's default location.
if(ASTRAL_BUILD_TESTS AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    function(_astral_set_test_shader_cache dir)
        get_property(tests DIRECTORY "${dir}" PROPERTY TESTS)
        if(tests)
            set_property(TEST ${tests} DIRECTORY "${dir}" APPEND PROPERTY ENVIRONMENT_MODIFICATION
                         "ASTRAL_SHADER_CACHE=set:${CMAKE_BINARY_DIR}/test-shader-cache")
        endif()
        get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
        foreach(sub IN LISTS subdirs)
            _astral_set_test_shader_cache("${sub}")
        endforeach()
    endfunction()
    _astral_set_test_shader_cache("${CMAKE_CURRENT_SOURCE_DIR}")
endif()
