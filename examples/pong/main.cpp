// 3D Pong: keep the ball in the tunnel. WASD / arrows or the mouse move your (blue) paddle; where
// the ball hits the paddle bends its path. First to 5. Esc quits.
//
//   pong                                   play
//   pong --demo                            the AI plays both sides
//   pong --capture out.ppm [--frames N] [--width W] [--height H]   AI vs AI, one image
//
// Everything you see is assets/court.scene and its prefabs (edit them while it runs); this file
// and pong.cpp are only the rules.

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
    explicit PongGame(bool demo) : demo_(demo) {}

    void on_start(Context& ctx) override {
        pong::install(ctx, demo_);
        ctx.load_scene("court.scene");
        if (ctx.window && !demo_) ctx.window->set_mouse_captured(true);
    }

    void on_update(Context& ctx) override {
        if (ctx.input.pressed(Key::Escape)) ctx.quit();
        const pong::Match* m = pong::match(ctx.world);
        if (!ctx.window || !m) return;
        const int key = m->player * 1000 + m->opponent + (m->game_over_timer > 0 ? 100000 : 0);
        if (key == shown_) return;
        shown_ = key;
        std::string title = "Astral Pong — " + std::to_string(m->player) + " : " + std::to_string(m->opponent);
        if (m->game_over_timer > 0) title += m->player > m->opponent ? "  — you win!" : "  — the AI wins";
        ctx.window->set_title(title);
    }

private:
    bool demo_;
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
    std::uint32_t frames = 150, width = 1280, height = 720;
    for (int i = 1; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : "0"; };
        if (!std::strcmp(argv[i], "--demo")) demo = true;
        else if (!std::strcmp(argv[i], "--capture")) out = next();
        else if (!std::strcmp(argv[i], "--frames")) frames = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--width")) width = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--height")) height = static_cast<std::uint32_t>(std::atoi(next()));
    }
    RenderSettings render;
    render.bloom_strength = 0.07f;
    App app({.title = "Astral Pong", .width = width, .height = height,
             .fixed_timestep = out ? 1.0f / 60.0f : 0.0f, // captures are deterministic
             .render = render, .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK, .offscreen = out != nullptr});
    PongGame game(demo || out);
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
