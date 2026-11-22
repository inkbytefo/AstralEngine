# Runs one golden image test: capture a frame with an example, then compare it with the reference.
# Variables: EXE, ARGS (space-separated, without --capture), OUT_DIR, NAME, REFERENCE, GOLDEN, THRESHOLD, MAX_BAD.
separate_arguments(ARGS UNIX_COMMAND "${ARGS}")
file(MAKE_DIRECTORY ${OUT_DIR})
set(actual ${OUT_DIR}/${NAME}.ppm)
file(REMOVE ${actual})

execute_process(COMMAND ${EXE} ${ARGS} --capture ${actual} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT EXISTS ${actual})
    if(err MATCHES "no GPU" OR out MATCHES "no GPU")
        message("SKIP: no GPU / Vulkan driver") # matched by SKIP_REGULAR_EXPRESSION
        return()
    endif()
    message(FATAL_ERROR "capture failed (${rc}):\n${out}\n${err}")
endif()

execute_process(COMMAND ${GOLDEN} ${actual} ${REFERENCE} --diff ${OUT_DIR}/${NAME}.diff.png
                        --threshold ${THRESHOLD} --max-bad ${MAX_BAD}
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}${err}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "golden image mismatch for ${NAME} (actual: ${actual}, reference: ${REFERENCE})")
endif()
