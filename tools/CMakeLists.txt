# astral-cook: cooks a game's assets and packs them (used by astral_add_game).
add_executable(astral_cook astral_cook/main.cpp)
set_target_properties(astral_cook PROPERTIES OUTPUT_NAME astral-cook EXPORT_NAME astral_cook)
target_link_libraries(astral_cook PRIVATE Astral::Assets)
astral_set_warnings(astral_cook)
if(ASTRAL_INSTALL)
    install(TARGETS astral_cook EXPORT AstralTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
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
endif()
