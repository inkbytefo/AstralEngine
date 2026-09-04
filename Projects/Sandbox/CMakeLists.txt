# Sandbox exe — saf runtime referans oyun uygulamasi.
add_executable(Sandbox
    src/main.cpp
)

target_link_libraries(Sandbox PRIVATE AstralEngine)

set_target_properties(Sandbox PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
