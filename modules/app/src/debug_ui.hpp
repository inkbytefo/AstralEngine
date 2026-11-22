#pragma once
// Internal: the engine's debug UI (Dear ImGui) — its renderer backend and the built-in panels.

#include <astral/app/app.hpp>

#include <cstdint>
#include <string>
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

    /// The built-in windows: performance, entities, inspector, render settings.
    /// `materials`: the .material library for the Materials panel (null: no panel content).
    void draw_panels(Context& ctx, class MaterialLibrary* materials = nullptr);

private:
    void update_textures();

    Renderer& renderer_;
    ImGuiContext* context_ = nullptr;
    std::vector<UiVertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::vector<UiCommand> commands_;
    bool keys_down_[256] = {};
    bool mouse_down_[3] = {};

    // Panel state.
    bool show_performance_ = true;
    bool show_scene_ = true;
    bool show_inspector_ = true;
    bool show_render_ = false;
    bool show_world_ = false;
    bool show_materials_ = false;
    bool show_demo_ = false;
    Entity selected_{};
    std::string filter_;
};

} // namespace astral
