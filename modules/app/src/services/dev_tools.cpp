#include "dev_tools.hpp"

#include "../debug_ui.hpp"
#include "../material_library.hpp"
#include "debug_overlays.hpp"

#include <astral/platform/input.hpp>
#include <astral/platform/window.hpp>
#include <astral/render/text.hpp>

namespace astral {

namespace {

// Headless apps never upload the SDF atlas texture; sharing one immutable default Font avoids
// re-rasterizing the full glyph atlas on every headless test/server App construction.
const Font* shared_headless_default_font() {
    static const std::unique_ptr<Font> font = Font::create(default_font_ttf());
    return font.get();
}

} // namespace

DevTools::DevTools(World& world, Window* window, Renderer* renderer, bool enable_debug_ui, bool builtin_panels,
                   std::string_view ini_path)
    : world_(world), window_(window), builtin_panels_(builtin_panels) {
    if (builtin_panels_) register_builtin_panels(panels_);
    if (renderer && enable_debug_ui) debug_ui_ = std::make_unique<DebugUi>(*renderer, window, ini_path);
    if (renderer) {
        owned_font_ = Font::create(default_font_ttf());
        if (owned_font_) owned_font_->upload(*renderer);
        font_ = owned_font_.get();
    } else {
        font_ = shared_headless_default_font();
    }
}

DevTools::~DevTools() {
    debug_ui_.reset(); // owns renderer textures
    font_ = nullptr;
    owned_font_.reset();
}

InputCapture DevTools::route_debug_input(const Input& raw_input, bool& show_debug_ui) {
    if (debug_ui_ && raw_input.pressed(Key::F1)) {
        show_debug_ui = !show_debug_ui;
        if (window_ && show_debug_ui && window_->mouse_captured()) { // the UI needs a cursor
            window_->set_mouse_captured(false);
            restore_mouse_capture_ = true;
        } else if (window_ && !show_debug_ui && restore_mouse_capture_) {
            window_->set_mouse_captured(true);
            restore_mouse_capture_ = false;
        }
    }
    if (debug_ui_ && show_debug_ui) { // what the UI used last frame is not the game's
        return {.keyboard = debug_ui_->wants_keyboard(), .mouse = debug_ui_->wants_mouse()};
    }
    return {};
}

void DevTools::draw_overlays(const Context::DebugShow& show, DebugDraw& debug, const WorldCellsOverlay* world_cells) const {
    draw_debug_overlays(world_, show, debug, world_cells);
}

void DevTools::build_ui(Game& game, Context& ctx, const Input& raw_input, float delta, gpu::Extent2D extent,
                        MaterialLibrary* materials) {
    ui_layer_ = {};
    const bool visible = debug_ui_ && ctx.show_debug_ui;
    if (window_ && text_input_ != (visible && debug_ui_->wants_text())) {
        text_input_ = !text_input_;
        window_->set_text_input(text_input_);
    }
    if (!visible) return;
    ASTRAL_PROFILE_SCOPE(&profiler_, "debug ui");
    debug_ui_->begin(raw_input, delta, extent, window_ ? window_->pixel_density() : 1.0f);
    if (builtin_panels_) debug_ui_->draw_panels(ctx, panels_, materials);
    game.on_debug_ui(ctx);
    debug_ui_->end();
    ui_layer_ = debug_ui_->layer();
}

std::span<const UiLayer> DevTools::overlay_layers(const UiCanvas& hud) {
    std::size_t n = 0;
    if (!hud.empty()) layers_[n++] = hud.layer();
    if (!ui_layer_.commands.empty()) layers_[n++] = ui_layer_;
    return std::span<const UiLayer>(layers_.data(), n);
}

} // namespace astral
