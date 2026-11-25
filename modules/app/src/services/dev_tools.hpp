#pragma once
// Internal to Astral::App: owns Profiler, DebugUi, DebugPanelRegistry, built-in Font, debug overlay drawing, and UI layer assembly.

#include "../panels/debug_panel.hpp"

#include <astral/app/app.hpp>
#include <astral/core/profiler.hpp>
#include <astral/gpu/types.hpp>

#include <array>
#include <memory>
#include <span>

namespace astral {

class DebugUi;
class Font;
class Input;
class MaterialLibrary;
class Renderer;
class Window;
class World;
struct WorldCellsOverlay;

struct InputCapture {
    bool keyboard = false;
    bool mouse = false;
};

class DevTools {
public:
    DevTools(World& world, Window* window, Renderer* renderer, bool enable_debug_ui, bool builtin_panels = true,
             std::string_view ini_path = {});
    ~DevTools();
    DevTools(const DevTools&) = delete;
    DevTools& operator=(const DevTools&) = delete;

    [[nodiscard]] Profiler& profiler() noexcept { return profiler_; }
    [[nodiscard]] const Font* font() const noexcept { return font_; }
    [[nodiscard]] DebugPanelRegistry& panels() noexcept { return panels_; }
    [[nodiscard]] const DebugPanelRegistry& panels() const noexcept { return panels_; }
    DebugPanel* register_panel(std::unique_ptr<DebugPanel> panel) { return panels_.add(std::move(panel)); }

    void begin_frame() { profiler_.begin_frame(); }
    void end_frame() { profiler_.end_frame(); }

    [[nodiscard]] InputCapture route_debug_input(const Input& raw_input, bool& show_debug_ui);
    void draw_overlays(const Context::DebugShow& show, DebugDraw& debug, const WorldCellsOverlay* world_cells) const;
    void build_ui(Game& game, Context& ctx, const Input& raw_input, float delta, gpu::Extent2D extent,
                  MaterialLibrary* materials);
    [[nodiscard]] std::span<const UiLayer> overlay_layers(const UiCanvas& hud);

private:
    World& world_;
    Window* window_;
    Profiler profiler_;
    DebugPanelRegistry panels_;
    std::unique_ptr<DebugUi> debug_ui_;
    std::unique_ptr<Font> owned_font_;
    const Font* font_ = nullptr;
    UiLayer ui_layer_{};
    std::array<UiLayer, 2> layers_{};
    bool restore_mouse_capture_ = false;
    bool text_input_ = false;
    bool builtin_panels_ = true;
};

} // namespace astral
