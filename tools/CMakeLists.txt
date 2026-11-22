# astral-cook: cooks a game's assets and packs them (used by astral_add_game).
add_executable(astral_cook astral_cook/main.cpp)
set_target_properties(astral_cook PROPERTIES OUTPUT_NAME astral-cook EXPORT_NAME astral_cook)
target_link_libraries(astral_cook PRIVATE Astral::Assets Astral::Script)
astral_set_warnings(astral_cook)
if(ASTRAL_INSTALL)
    install(TARGETS astral_cook EXPORT AstralTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# astral-features: checks the boundaries of a game's feature packages (ADR-0023).
add_executable(astral_features astral_features/main.cpp)
set_target_properties(astral_features PROPERTIES OUTPUT_NAME astral-features EXPORT_NAME astral_features)
target_link_libraries(astral_features PRIVATE Astral::App)
astral_set_warnings(astral_features)
if(ASTRAL_INSTALL)
    install(TARGETS astral_features EXPORT AstralTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# astral-luau-check: type-checks Luau scripts against astral.d.luau with Luau's analyzer (tests only: it builds
# Luau's Analysis library, about a minute).
option(ASTRAL_LUAU_CHECK "Build astral-luau-check (type-checks scripts against the generated definitions)" ${ASTRAL_BUILD_TESTS})
if(ASTRAL_LUAU_CHECK)
    FetchContent_GetProperties(luau)
    set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${luau_SOURCE_DIR} ${CMAKE_BINARY_DIR}/luau-analysis EXCLUDE_FROM_ALL SYSTEM)
    add_executable(astral_luau_check astral_luau_check/main.cpp)
    set_target_properties(astral_luau_check PROPERTIES OUTPUT_NAME astral-luau-check)
    target_link_libraries(astral_luau_check PRIVATE Luau.Analysis)
    target_compile_features(astral_luau_check PRIVATE cxx_std_17)
endif()

if(ASTRAL_BUILD_TESTS)
    # Smoke test of the command line: cook + pack a small folder, then cook again (nothing to do).
    set(out ${CMAKE_CURRENT_BINARY_DIR}/cli_test)
    add_test(NAME astral_cook_cli
        COMMAND ${CMAKE_COMMAND} -E env --
                $<TARGET_FILE:astral_cook> ${CMAKE_SOURCE_DIR}/examples/scene_viewer/assets/prefabs -o ${out}/cooked
                --pack ${out}/prefabs.apak -q)
    add_test(NAME astral_cook_cli_usage COMMAND $<TARGET_FILE:astral_cook>)
    set_tests_properties(astral_cook_cli_usage PROPERTIES WILL_FAIL TRUE)

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME check_sizes_self_test
            COMMAND Python3::Interpreter -B -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p "test_check_sizes.py" -v)
    else()
        message(STATUS "Python3 interpreter not found: skipping check_sizes_self_test")
    endif()
endif()
