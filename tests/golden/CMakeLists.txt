# Golden image tests (ADR-0031): render a scene offscreen and compare against lavapipe references/*.png.
#   golden    — lavapipe/llvmpipe only (strict per-pixel tolerance; CI gate; update: ASTRAL_UPDATE_GOLDEN=1 ctest -L '^golden$')
#   golden-hw — non-software GPUs (luminance SSIM + strong-diff pixel cap; never updates references)
include(FetchContent)
FetchContent_GetProperties(stb) # populated by modules/assets (header-only)

add_library(astral_golden_compare STATIC image_compare.cpp device_info.cpp image_io.cpp)
target_include_directories(astral_golden_compare PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(astral_golden_compare SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_compile_features(astral_golden_compare PUBLIC cxx_std_20)
astral_set_warnings(astral_golden_compare)

add_executable(astral_golden_tests ${CMAKE_SOURCE_DIR}/modules/core/tests/test_main.cpp image_compare_test.cpp)
target_link_libraries(astral_golden_tests PRIVATE astral_golden_compare doctest::doctest)
target_compile_definitions(astral_golden_tests PRIVATE ASTRAL_GOLDEN_REFERENCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/references")
astral_set_warnings(astral_golden_tests)
add_test(NAME astral_golden_tests COMMAND astral_golden_tests)

add_executable(astral_golden golden_main.cpp)
set_target_properties(astral_golden PROPERTIES OUTPUT_NAME astral-golden)
target_link_libraries(astral_golden PRIVATE astral_golden_compare)
astral_set_warnings(astral_golden)

# astral_add_golden_test(<name> TARGET <example> [ARGS ...] [THRESHOLD n] [MAX_BAD f]
#                        [MIN_SSIM f] [STRONG_THRESHOLD n] [MAX_STRONG_BAD f])
# Captures at a small fixed size; reference: references/<name>.png. Registers both golden (software)
# and golden-hw (hardware) tests sharing a per-scene RESOURCE_LOCK so parallel ctest (-j) never races
# on out/<name>.ppm.
function(astral_add_golden_test name)
    cmake_parse_arguments(G "" "TARGET;THRESHOLD;MAX_BAD;MIN_SSIM;STRONG_THRESHOLD;MAX_STRONG_BAD" "ARGS" ${ARGN})
    if(NOT G_THRESHOLD)
        set(G_THRESHOLD 8)
    endif()
    if(NOT G_MAX_BAD)
        set(G_MAX_BAD 0.005)
    endif()
    if(NOT G_MIN_SSIM)
        set(G_MIN_SSIM 0.95)
    endif()
    if(NOT G_STRONG_THRESHOLD)
        set(G_STRONG_THRESHOLD 32)
    endif()
    if(NOT G_MAX_STRONG_BAD)
        set(G_MAX_STRONG_BAD 0.025)
    endif()
    list(JOIN G_ARGS " " args)
    set(skip_regex "SKIP: (no GPU|software golden|hardware golden)")

    add_test(NAME golden_${name}
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:${G_TARGET}> "-DARGS=${args}"
                -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/out -DNAME=${name}
                -DREFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/references/${name}.png
                -DGOLDEN=$<TARGET_FILE:astral_golden> -DMODE=software
                -DTHRESHOLD=${G_THRESHOLD} -DMAX_BAD=${G_MAX_BAD}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/RunGolden.cmake)
    set_tests_properties(golden_${name} PROPERTIES
        LABELS golden
        TIMEOUT 600
        RESOURCE_LOCK golden_scene_${name}
        SKIP_REGULAR_EXPRESSION "${skip_regex}")

    add_test(NAME golden_hw_${name}
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:${G_TARGET}> "-DARGS=${args}"
                -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/out -DNAME=${name}
                -DREFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/references/${name}.png
                -DGOLDEN=$<TARGET_FILE:astral_golden> -DMODE=hardware
                -DMIN_SSIM=${G_MIN_SSIM} -DSTRONG_THRESHOLD=${G_STRONG_THRESHOLD}
                -DMAX_STRONG_BAD=${G_MAX_STRONG_BAD}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/RunGolden.cmake)
    set_tests_properties(golden_hw_${name} PROPERTIES
        LABELS golden-hw
        TIMEOUT 600
        RESOURCE_LOCK golden_scene_${name}
        SKIP_REGULAR_EXPRESSION "${skip_regex}")
endfunction()

# The showcases have no golden tests yet: references are made on lavapipe (CI) only, see docs/status.md.
# references/scene_viewer.png stays as the fixture of image_compare_test.
if(TARGET pong)
    astral_add_golden_test(pong TARGET pong ARGS --frames 90 --width 320 --height 180)
endif()
if(TARGET blank)
    astral_add_golden_test(blank TARGET blank ARGS --frames 30 --width 320 --height 180)
endif()
if(TARGET hello_triangle)
    astral_add_golden_test(hello_triangle TARGET hello_triangle ARGS --frames 8 --width 320 --height 180)
endif()
if(TARGET spinning_cubes)
    astral_add_golden_test(spinning_cubes TARGET spinning_cubes ARGS --frames 30 --width 320 --height 180)
endif()
