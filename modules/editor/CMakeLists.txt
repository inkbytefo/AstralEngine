astral_add_module(NAME editor
    SOURCES
        src/workspace.cpp
        src/panels.cpp
    DEPS
        Astral::Core
        Astral::World
        Astral::Assets
        Astral::App
        Astral::Editor_core
        Astral::ImGui)
