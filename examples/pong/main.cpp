// 3D Pong: keep the ball in the tunnel. WASD / arrows or the mouse move your (blue) paddle; where
// the ball hits the paddle bends its path. First to 5. Esc quits.
//
//   pong                                   play
//   pong --demo                            the AI plays both sides
//   pong --script-api <folder>             also writes astral.d.luau and API.md (the script API)
//   pong --capture out.ppm [--frames N] [--width W] [--height H]   AI vs AI, one image
//
// Everything you see is assets/court.scene and its prefabs (edit them while it runs); the rules are
// Luau (assets/features/pong_rules/*.luau, reloaded when you save) and two small C++ features.

#include "pong.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace astral;

namespace {

class PongGame final : public Game {
public:
    PongGame(bool demo, std::string script_api) : demo_(demo), script_api_(std::move(script_api)) {}

    void on_start(Context& ctx) override {
        if (!pong::install(ctx)) {
            ctx.quit();
            return;
        }
        ctx.load_scene("court.scene");
        if (demo_) pong::set_player_ai(ctx.world, true);
        if (!script_api_.empty()) ctx.write_script_api(script_api_);
        if (ctx.window && !demo_) ctx.window->set_mouse_captured(true);
    }

    void on_update(Context& ctx) override {
        if (ctx.input.pressed(Key::Escape)) ctx.quit();
        const pong::Match* m = pong::match(ctx.world);
        if (!m) return;
        draw_hud(ctx, *m);
        if (!ctx.window) return;
        const int key = m->player * 1000 + m->opponent + (m->game_over_timer > 0 ? 100000 : 0);
        if (key == shown_) return;
        shown_ = key;
        std::string title = "Astral Pong — " + std::to_string(m->player) + " : " + std::to_string(m->opponent);
        if (m->game_over_timer > 0) title += m->player > m->opponent ? "  — you win!" : "  — the AI wins";
        ctx.window->set_title(title);
    }

private:
    /// Score at the top, the result when a match ends, controls at the bottom.
    void draw_hud(Context& ctx, const pong::Match& m) const {
        const Font& font = *ctx.font;
        const Vec2 screen = ctx.screen_size();
        const float unit = screen.y / 720.0f; // the HUD scales with the window
        const Vec4 blue{0.45f, 0.72f, 1.0f, 1}, red{1.0f, 0.5f, 0.38f, 1}, white{1, 1, 1, 1};
        const float y = 18 * unit, size = 54 * unit;
        ctx.hud.text(font, std::to_string(m.player), {screen.x * 0.5f - 30 * unit, y}, {.size = size, .color = blue, .align = TextAlign::Right, .shadow = true});
        ctx.hud.text(font, ":", {screen.x * 0.5f, y}, {.size = size, .color = white, .align = TextAlign::Center, .shadow = true});
        ctx.hud.text(font, std::to_string(m.opponent), {screen.x * 0.5f + 30 * unit, y}, {.size = size, .color = red, .shadow = true});
        if (m.game_over_timer > 0) {
            const bool won = m.player > m.opponent;
            const float h = 120 * unit;
            ctx.hud.rect({0, screen.y * 0.5f - h * 0.5f}, {screen.x, screen.y * 0.5f + h * 0.5f}, {0, 0, 0, 0.55f});
            ctx.hud.text(font, won ? "KAZANDIN!" : "YAPAY ZEKÂ KAZANDI", {screen.x * 0.5f, screen.y * 0.5f - 34 * unit},
                         {.size = 64 * unit, .color = won ? blue : red, .align = TextAlign::Center, .shadow = true});
        }
        if (!demo_) {
            ctx.hud.text(font, "WASD / oklar / fare: raket   •   F1: debug   •   Esc: çıkış", {screen.x * 0.5f, screen.y - 34 * unit},
                         {.size = 18 * unit, .color = {1, 1, 1, 0.7f}, .align = TextAlign::Center});
        }
    }

    bool demo_;
    std::string script_api_;
    int shown_ = -1;
};

bool write_ppm(const char* path, const std::vector<std::uint8_t>& rgba, std::uint32_t w, std::uint32_t h) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (std::size_t p = 0; p < std::size_t{w} * h; ++p) std::fwrite(&rgba[p * 4], 1, 3, f);
    std::fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const char* out = nullptr;
    bool demo = false;
    std::string script_api;
    std::uint32_t frames = 150, width = 1280, height = 720;
    for (int i = 1; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : "0"; };
        if (!std::strcmp(argv[i], "--demo")) demo = true;
        else if (!std::strcmp(argv[i], "--capture")) out = next();
        else if (!std::strcmp(argv[i], "--script-api")) script_api = next();
        else if (!std::strcmp(argv[i], "--frames")) frames = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--width")) width = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--height")) height = static_cast<std::uint32_t>(std::atoi(next()));
    }
    RenderSettings render;
    render.bloom_strength = 0.07f;
    App app({.title = "Astral Pong", .width = width, .height = height,
             .fixed_timestep = out ? 1.0f / 60.0f : 0.0f, // captures are deterministic
             .render = render, .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK, .offscreen = out != nullptr});
    PongGame game(demo || out, script_api);
    if (!out) return app.run(game);

    app.run_frames(game, frames);
    std::vector<std::uint8_t> rgba;
    if (!app.read_frame(rgba) || !write_ppm(out, rgba, width, height)) {
        std::fprintf(stderr, "capture failed (no GPU?)\n");
        return 1;
    }
    std::printf("wrote %s\n", out);
    return 0;
}
