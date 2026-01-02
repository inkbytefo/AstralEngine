# /**********************************************************************
#  * Astral Engine - Examples CMake Configuration
#  * Purpose: Modern CMake configuration for example applications
#  * Author: Astral Engine Development Team
#  * Version: 0.2.0 (Refactored)
#  * Date: 2025-12-21
#  **********************************************************************/

# RenderTest Application
add_executable(RenderTest RenderTest.cpp)
target_link_libraries(RenderTest PRIVATE AstralEngine)
configure_astral_target(RenderTest)

# BMW RenderTest Application
add_executable(BMWRenderTest BMW_RenderTest.cpp)
target_link_libraries(BMWRenderTest PRIVATE AstralEngine)
configure_astral_target(BMWRenderTest)

set_target_properties(RenderTest PROPERTIES
    OUTPUT_NAME "RenderTest"
    DESCRIPTION "Astral Engine Render Module Verification Test"
    VERSION ${PROJECT_VERSION}
    WIN32_EXECUTABLE OFF
    DEBUG_POSTFIX "_d"
    RELWITHDEBINFO_POSTFIX "_rd"
)

set_target_properties(BMWRenderTest PROPERTIES
    OUTPUT_NAME "BMWRenderTest"
    DESCRIPTION "Astral Engine BMW Model Rendering Test"
    VERSION ${PROJECT_VERSION}
    WIN32_EXECUTABLE OFF
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
