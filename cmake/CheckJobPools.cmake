# cmake -DBUILD_DIR=<ninja build folder> -P CheckJobPools.cmake
# Fails unless the Ninja build declares the heavy_compile pool and compiles Jolt sources in it.
set(build_ninja "${BUILD_DIR}/build.ninja")
set(rules_ninja "${BUILD_DIR}/CMakeFiles/rules.ninja") # CMake writes `pool` blocks here
foreach(file IN ITEMS "${build_ninja}" "${rules_ninja}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "not found: ${file}")
    endif()
endforeach()
file(READ "${rules_ninja}" rules)
file(READ "${build_ninja}" ninja)
if(NOT "${rules}${ninja}" MATCHES "pool heavy_compile\r?\n  depth = [1-9][0-9]*")
    message(FATAL_ERROR "the Ninja build declares no heavy_compile pool")
endif()
# A Jolt object (e.g. Jolt/Physics/PhysicsSystem.cpp) must name the pool in its build edge.
string(REGEX MATCH "build [^\n]*PhysicsSystem\\.cpp\\.obj?[^\n]*\n([ ][^\n]*\n)*" jolt_edge "${ninja}")
if(jolt_edge STREQUAL "")
    message(FATAL_ERROR "no build edge for Jolt PhysicsSystem.cpp")
endif()
if(NOT jolt_edge MATCHES "pool = heavy_compile")
    message(FATAL_ERROR "Jolt sources are not compiled in the heavy_compile pool")
endif()
message(STATUS "heavy_compile pool: ok")
