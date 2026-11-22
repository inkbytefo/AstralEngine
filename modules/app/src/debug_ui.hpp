#pragma once
// Internal: the engine's debug UI (Dear ImGui) — its renderer backend and panel menu/dispatch loop.

#include "panels/debug_panel.hpp"

#include <astral/app/app.hpp>

#include <cstdint>
#include <vector>

struct ImGuiContext;

namespace astral {

class DebugUi {
public:
    explicit DebugUi(Renderer& renderer);
    ~DebugUi();
    DebugUi(const DebugUi&) = delete;
    DebugUi& operator=(const DebugUi&) = delete;

    /// Starts an ImGui frame from this frame's input (positions in window coordinates).
    void begin(const Input& input, float delta, gpu::Extent2D size, float pixel_density);
    /// Ends it: honours ImGui's texture requests and converts the draw lists.
    void end();
    [[nodiscard]] UiLayer layer() const;

    [[nodiscard]] bool wants_mouse() const;
    [[nodiscard]] bool wants_keyboard() const;
    [[nodiscard]] bool wants_text() const;

    /// Draws the main menu bar and every open panel in `panels`.
    /// `materials`: the .material library for the Materials panel (null: no panel content).
    void draw_panels(Context& ctx, DebugPanelRegistry& panels, class MaterialLibrary* materials = nullptr);

private:
    void update_textures();

    Renderer& renderer_;
    ImGuiContext* context_ = nullptr;
    std::vector<UiVertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::vector<UiCommand> commands_;
    bool keys_down_[256] = {};
    bool mouse_down_[3] = {};
    DebugPanelState panel_state_{};
};

} // namespace astral
