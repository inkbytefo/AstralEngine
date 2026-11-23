#include "game.hpp"

#include "gallery.hpp"

namespace material_showcase {

void ShowcaseGame::on_start(astral::Context& ctx) { spawn_gallery(ctx); }

void ShowcaseGame::on_update(astral::Context& ctx) {
    if (ctx.renderer && ctx.input.pressed(astral::Key::P)) {
        astral::RenderSettings s = ctx.renderer->settings();
        s.path_tracer = !s.path_tracer;
        ctx.renderer->set_settings(s);
    }
}

} // namespace material_showcase
