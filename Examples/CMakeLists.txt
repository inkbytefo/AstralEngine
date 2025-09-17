# /**********************************************************************
#  * Astral Engine - Examples CMake Configuration
#  * Purpose: Modern CMake configuration for example applications
#  * Author: Astral Engine Development Team
#  * Version: 0.1.0-alpha
#  * Date: 2025-09-17
#  * Signature: inkbytefo
#  **********************************************************************/

# Sandbox uygulaması için bir executable hedefi oluştur
add_executable(Sandbox Sandbox.cpp)

# Sandbox hedefini, ana motor kütüphanesi olan AstralEngine'e bağla
target_link_libraries(Sandbox PRIVATE AstralEngine)

# configure_astral_target fonksiyonunu Sandbox için de çağırarak
# gerekli tüm ayarları (include'lar, define'lar, linklemeler) yap
configure_astral_target(Sandbox)

# Target için IDE özelliklerini ayarla
set_target_properties(Sandbox PROPERTIES
    OUTPUT_NAME "Sandbox$<$<CONFIG:Debug>:_Debug>"
    DESCRIPTION "Astral Engine Sandbox Application"
    VERSION ${PROJECT_VERSION}
    $<$<PLATFORM_ID:Windows>:WIN32_EXECUTABLE OFF>
    $<$<CONFIG:Debug>:DEBUG_POSTFIX "_d">
    $<$<CONFIG:RelWithDebInfo>:RELWITHDEBINFO_POSTFIX "_rd">
)

# Copy assets for Sandbox application
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Sandbox>/Assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:Sandbox>/Assets
    COMMENT "Copying assets to Sandbox output directory"
)

# Copy compiled shaders to Sandbox output directory (if Vulkan is enabled)
if(ASTRAL_HAS_VULKAN)
    add_custom_command(TARGET Sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Sandbox>/Assets/Shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
            $<TARGET_FILE_DIR:Sandbox>/Assets/Shaders/
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
            $<TARGET_FILE_DIR:Sandbox>/Assets/Shaders/
        COMMENT "Copying compiled shaders to Sandbox output directory"
    )
endif()

message(STATUS "Sandbox example application target created")
