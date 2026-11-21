# astral_add_module(NAME <name> SOURCES <src...> [DEPS <Astral::X...>] [PRIVATE_DEPS <...>])
#   Creates static library astral_<name> with alias Astral::<Name>.
#   Public headers live in modules/<name>/include/astral/<name>/.
#   DEPS are PUBLIC (part of the API), PRIVATE_DEPS stay hidden (third-party libs).
function(astral_add_module)
    cmake_parse_arguments(ARG "" "NAME" "SOURCES;DEPS;PRIVATE_DEPS" ${ARGN})
    set(target astral_${ARG_NAME})
    string(SUBSTRING ${ARG_NAME} 0 1 first)
    string(TOUPPER ${first} first)
    string(SUBSTRING ${ARG_NAME} 1 -1 rest)
    set(export_name "${first}${rest}")

    add_library(${target} STATIC ${ARG_SOURCES})
    add_library(Astral::${export_name} ALIAS ${target})
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${export_name})
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_include_directories(${target}
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(ARG_DEPS)
        target_link_libraries(${target} PUBLIC ${ARG_DEPS})
    endif()
    if(ARG_PRIVATE_DEPS)
        target_link_libraries(${target} PRIVATE ${ARG_PRIVATE_DEPS})
    endif()
    astral_set_warnings(${target})
    set_property(GLOBAL APPEND PROPERTY ASTRAL_MODULES ${target})
    if(ASTRAL_INSTALL)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)
    endif()
endfunction()

# astral_add_tests(MODULE <name> SOURCES <src...>)
#   Builds astral_<name>_tests (doctest) and registers it with ctest.
function(astral_add_tests)
    cmake_parse_arguments(ARG "" "MODULE" "SOURCES" ${ARGN})
    if(NOT ASTRAL_BUILD_TESTS)
        return()
    endif()
    set(target astral_${ARG_MODULE}_tests)
    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE astral_${ARG_MODULE} doctest::doctest)
    astral_set_warnings(${target})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 120)
endfunction()
