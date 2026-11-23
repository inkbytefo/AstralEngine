#include <astral/editor/workspace.hpp>

int main() {
    astral::App app({
        .title = "Astral World Editor (E0)",
        .width = 1440,
        .height = 900,
        .asset_root = ASTRAL_EDITOR_ASSETS,
        .debug_ui = true,
        .debug_ui_builtin_panels = false,
    });
    astral::editor::EditorWorkspace editor({
        .asset_root = ASTRAL_EDITOR_ASSETS,
        .initial_document = "scenes/workshop.scene",
    });
    return app.run(editor);
}
