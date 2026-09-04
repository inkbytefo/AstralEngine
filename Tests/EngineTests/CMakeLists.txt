add_executable(EngineTests
    src/main.cpp
    src/EcsTests.cpp
    src/PhysicsTests.cpp
    src/IdentityTests.cpp
    src/SceneTests.cpp
    src/SerializationTests.cpp
    src/GpuSmokeTest.cpp
)

target_include_directories(EngineTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(EngineTests PRIVATE
    AstralEngine
)

set_target_properties(EngineTests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
