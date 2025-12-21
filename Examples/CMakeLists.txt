# /**********************************************************************
#  * Astral Engine - Examples CMake Configuration
#  * Purpose: Modern CMake configuration for example applications
#  * Author: Astral Engine Development Team
#  * Version: 0.2.0 (Refactored)
#  * Date: 2025-12-21
#  **********************************************************************/

# RenderTest Application
add_executable(RenderTest RenderTest.cpp)

# Link against the main engine library
target_link_libraries(RenderTest PRIVATE AstralEngine)

# Apply standard configuration (includes, defines, etc.)
configure_astral_target(RenderTest)

set_target_properties(RenderTest PROPERTIES
    OUTPUT_NAME "RenderTest"
    DESCRIPTION "Astral Engine Render Module Verification Test"
    VERSION ${PROJECT_VERSION}
    WIN32_EXECUTABLE OFF # Keep console for logging
    DEBUG_POSTFIX "_d"
    RELWITHDEBINFO_POSTFIX "_rd"
)

# Copy assets for RenderTest application
# add_custom_command(TARGET RenderTest POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:RenderTest>/Assets
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#        ${CMAKE_SOURCE_DIR}/Assets
#        $<TARGET_FILE_DIR:RenderTest>/Assets
#    COMMENT "Copying assets to RenderTest output directory"
# )

message(STATUS "RenderTest application target created")
