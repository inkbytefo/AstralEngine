# EmptyGameTemplate — Minimal AstralEngine oyun sablonu.
add_executable(EmptyGameTemplate
    src/main.cpp
)

target_link_libraries(EmptyGameTemplate PRIVATE AstralEngine)

set_target_properties(EmptyGameTemplate PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
