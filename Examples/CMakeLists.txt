# /**********************************************************************
#  * Astral Engine - Examples CMake Configuration
#  * Purpose: Modern CMake configuration for example applications
#  * Author: Astral Engine Development Team
#  * Version: 0.1.0-alpha
#  * Date: 2025-09-17
#  * Signature: inkbytefo
#  **********************************************************************/

# Basic Example uygulaması için bir executable hedefi oluştur
add_executable(BasicExample Sandbox.cpp)

# BasicExample hedefini, ana motor kütüphanesi olan AstralEngine'e bağla
target_link_libraries(BasicExample PRIVATE AstralEngine)

# configure_astral_target fonksiyonunu BasicExample için de çağırarak
# gerekli tüm ayarları (include'lar, define'lar, linklemeler) yap
configure_astral_target(BasicExample)

# Target için IDE özelliklerini ayarla
set_target_properties(BasicExample PROPERTIES
    OUTPUT_NAME "BasicExample$<$<CONFIG:Debug>:_Debug>"
    DESCRIPTION "Astral Engine Basic Example Application"
    VERSION ${PROJECT_VERSION}
    $<$<PLATFORM_ID:Windows>:WIN32_EXECUTABLE OFF>
    $<$<CONFIG:Debug>:DEBUG_POSTFIX "_d">
    $<$<CONFIG:RelWithDebInfo>:RELWITHDEBINFO_POSTFIX "_rd">
)

# Copy assets for BasicExample application
add_custom_command(TARGET BasicExample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:BasicExample>/Assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:BasicExample>/Assets
    COMMENT "Copying assets to BasicExample output directory"
)

# Copy compiled shaders to BasicExample output directory (if Vulkan is enabled)
if(ASTRAL_HAS_VULKAN)
    add_custom_command(TARGET BasicExample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:BasicExample>/Assets/Shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
            $<TARGET_FILE_DIR:BasicExample>/Assets/Shaders/
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
            $<TARGET_FILE_DIR:BasicExample>/Assets/Shaders/
        COMMENT "Copying compiled shaders to BasicExample output directory"
    )
endif()

message(STATUS "BasicExample application target created")
