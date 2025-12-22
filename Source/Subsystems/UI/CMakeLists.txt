target_sources(AstralEngine PRIVATE
    UISubsystem.cpp
    UISubsystem.h
)

if(ASTRAL_USE_IMGUI)
    # Add ImGui Core Sources
    target_sources(AstralEngine PRIVATE
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    )

    # Add ImGui Backends
    target_sources(AstralEngine PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )
    
    # Include Directories
    target_include_directories(AstralEngine PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
endif()
