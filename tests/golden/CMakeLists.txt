# Golden image tests: render a scene offscreen, compare with a stored reference (tolerant), write a diff
# image on failure. References live in references/*.png. Update: ASTRAL_UPDATE_GOLDEN=1 ctest -L golden.
add_library(astral_golden_compare STATIC image_compare.cpp)
target_include_directories(astral_golden_compare PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(astral_golden_compare PUBLIC cxx_std_20)
astral_set_warnings(astral_golden_compare)

add_executable(astral_golden_tests ${CMAKE_SOURCE_DIR}/modules/core/tests/test_main.cpp image_compare_test.cpp)
target_link_libraries(astral_golden_tests PRIVATE astral_golden_compare doctest::doctest)
astral_set_warnings(astral_golden_tests)
add_test(NAME astral_golden_tests COMMAND astral_golden_tests)

include(FetchContent)
FetchContent_GetProperties(stb) # populated by modules/assets (header-only)
add_executable(astral_golden golden_main.cpp)
set_target_properties(astral_golden PROPERTIES OUTPUT_NAME astral-golden)
target_link_libraries(astral_golden PRIVATE astral_golden_compare)
target_include_directories(astral_golden SYSTEM PRIVATE ${stb_SOURCE_DIR})
astral_set_warnings(astral_golden)

# astral_add_golden_test(<name> TARGET <example> [ARGS ...] [THRESHOLD n] [MAX_BAD f])
# Captures at a small fixed size; reference: references/<name>.png. Label: golden.
function(astral_add_golden_test name)
    cmake_parse_arguments(G "" "TARGET;THRESHOLD;MAX_BAD" "ARGS" ${ARGN})
    if(NOT G_THRESHOLD)
        set(G_THRESHOLD 8)
    endif()
    if(NOT G_MAX_BAD)
        set(G_MAX_BAD 0.005)
    endif()
    list(JOIN G_ARGS " " args)
    add_test(NAME golden_${name}
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:${G_TARGET}> "-DARGS=${args}"
                -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/out -DNAME=${name}
                -DREFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/references/${name}.png
                -DGOLDEN=$<TARGET_FILE:astral_golden> -DTHRESHOLD=${G_THRESHOLD} -DMAX_BAD=${G_MAX_BAD}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/RunGolden.cmake)
    set_tests_properties(golden_${name} PROPERTIES LABELS golden TIMEOUT 600 SKIP_REGULAR_EXPRESSION "SKIP: no GPU")
endfunction()

astral_add_golden_test(scene_viewer TARGET scene_viewer ARGS --frames 24 --width 320 --height 180)
astral_add_golden_test(rt_showcase TARGET rt_showcase ARGS --frames 32 --width 320 --height 180)
astral_add_golden_test(pong TARGET pong ARGS --frames 90 --width 320 --height 180)
