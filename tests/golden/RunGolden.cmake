# Runs one golden image test: capture a frame with an example, then compare it with the reference.
# Variables: EXE, ARGS (space-separated, without --capture), OUT_DIR, NAME, REFERENCE, GOLDEN, MODE
#   software mode: THRESHOLD, MAX_BAD
#   hardware mode: MIN_SSIM, STRONG_THRESHOLD, MAX_STRONG_BAD
separate_arguments(ARGS UNIX_COMMAND "${ARGS}")
file(MAKE_DIRECTORY ${OUT_DIR})
set(actual ${OUT_DIR}/${NAME}.ppm)
file(REMOVE ${actual})

execute_process(COMMAND ${EXE} ${ARGS} --capture ${actual}
                RESULT_VARIABLE rc OUTPUT_VARIABLE cap_out ERROR_VARIABLE cap_err)
if(NOT rc EQUAL 0 OR NOT EXISTS ${actual})
    if(cap_err MATCHES "no GPU" OR cap_out MATCHES "no GPU")
        message("SKIP: no GPU / Vulkan driver") # matched by SKIP_REGULAR_EXPRESSION
        return()
    endif()
    message(FATAL_ERROR "capture failed (${rc}):\n${cap_out}\n${cap_err}")
endif()

string(REGEX MATCH "capture device:[^\r\n]*" device_line "${cap_out}")

if(MODE STREQUAL "hardware")
    # Hardware runs must never overwrite software references even if ASTRAL_UPDATE_GOLDEN is set.
    set(ENV{ASTRAL_UPDATE_GOLDEN} "")
    set(compare_cmd ${GOLDEN} ${actual} ${REFERENCE} --mode hardware --device-line "${device_line}"
                    --diff ${OUT_DIR}/${NAME}.diff.png
                    --min-ssim ${MIN_SSIM} --strong-threshold ${STRONG_THRESHOLD} --max-strong-bad ${MAX_STRONG_BAD})
else()
    set(compare_cmd ${GOLDEN} ${actual} ${REFERENCE} --mode software --device-line "${device_line}"
                    --diff ${OUT_DIR}/${NAME}.diff.png
                    --threshold ${THRESHOLD} --max-bad ${MAX_BAD})
endif()

execute_process(COMMAND ${compare_cmd} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}${err}")
if(rc EQUAL 125)
    return() # SKIP: ... printed above and matched by SKIP_REGULAR_EXPRESSION
endif()
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "golden image mismatch for ${NAME} (actual: ${actual}, reference: ${REFERENCE})")
endif()
