
project(AstralTests)

enable_testing()

add_executable(AstralTests
    SceneSerializerTest.cpp
)

target_link_libraries(AstralTests PRIVATE
    AstralEngine
    Catch2::Catch2WithMain
)

target_include_directories(AstralTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
)

# Copy Assets if needed (not needed for simple serialization test unless checking asset paths)
# But we might need to verify file creation in CWD.

add_test(NAME AstralTests COMMAND AstralTests)
