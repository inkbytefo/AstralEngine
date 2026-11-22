// Open world demo (M9, ADR-0021): fly over a generated 4 x 4 km map whose cells stream in and out
// around the camera, with a floating origin and a "night" data layer.
//
//   open_world                      fly: WASD, Q/E down/up, Shift fast, right mouse look, N night, F1 panels
//   open_world --capture out.ppm [--frames 40] [--width 1280] [--height 720] [--night]
//
// The map is made by tools/make_test_world.py at build time and cooked into cells.

#include <astral/astral.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace astral;

namespace {

class OpenWorld final : public Game {
public:
    OpenWorld(bool capture, bool night) : capture_(capture), night_(night) {}

    void on_start(Context& ctx) override {
        if (!ctx.open_world("maps/test.world")) {
            ctx.quit();
            return;
        }
        ctx.sun.direction = normalize(Vec3{-0.4f, -0.55f, -0.35f});
        set_night(ctx, night_);
        // Start near one corner, 40 m up, looking into the world; the first frames already cross the
        // 2 km origin threshold (the start is 2.4 km from zero).
        camera_ = ctx.world.create(Transform{.position = {1700, 40, 1700}}, Camera{.far_plane = 2500.0f}, StreamingSource{});
        yaw_ = radians(-135.0f);
        pitch_ = radians(-12.0f);
        apply_rotation(ctx);
        ctx.finish_world_loading(); // a loading screen would show here
    }

    void on_update(Context& ctx) override {
        if (!camera_.valid()) return;
        const Input& in = ctx.input;
        if (in.pressed(Key::Escape)) ctx.quit();
        if (in.pressed(Key::N)) {
            night_ = !night_;
            set_night(ctx, night_);
        }
        const float dt = ctx.time.delta;
        Vec3 move{};
        if (capture_) {
            move = {0, 0, -1}; // straight ahead at a steady speed
        } else {
            if (in.down(MouseButton::Right)) {
                yaw_ -= in.mouse_delta().x * 0.003f;
                pitch_ = std::fmax(-1.4f, std::fmin(1.4f, pitch_ - in.mouse_delta().y * 0.003f));
                apply_rotation(ctx);
            }
            move.x = (in.down(Key::D) ? 1.0f : 0.0f) - (in.down(Key::A) ? 1.0f : 0.0f);
            move.z = (in.down(Key::S) ? 1.0f : 0.0f) - (in.down(Key::W) ? 1.0f : 0.0f);
            move.y = (in.down(Key::E) ? 1.0f : 0.0f) - (in.down(Key::Q) ? 1.0f : 0.0f);
        }
        const float speed = (in.down(Key::LeftShift) ? 400.0f : 80.0f) * (capture_ ? 1.5f : 1.0f);
        Transform* t = ctx.world.get<Transform>(camera_);
        const Vec3 step = t->rotation * move;
        t->position = {t->position.x + step.x * speed * dt, t->position.y + step.y * speed * dt,
                       t->position.z + step.z * speed * dt};
        // Captures are image tests: every cell in range is in the picture, whatever the timing.
        if (capture_) ctx.finish_world_loading();

        if (const StreamingStats* s = ctx.world_streaming()) {
            const WorldPosition w = ctx.world_origin();
            const Vec2 screen = ctx.screen_size();
            const float unit = screen.y / 720.0f;
            char line[160];
            std::snprintf(line, sizeof line, "cells %u  loading %u  |  origin %.0f, %.0f  |  %s", s->attached, s->loading + s->waiting,
                          w.x, w.z, night_ ? "night (N)" : "day (N)");
            ctx.hud.text(*ctx.font, line, {16 * unit, 16 * unit}, {.size = 18 * unit, .color = {1, 1, 1, 0.85f}, .shadow = true});
        }
    }

private:
    /// Night: the lamp layer comes on, the sun becomes moonlight, the sky goes dark.
    static void set_night(Context& ctx, bool night) {
        ctx.world_layers()->set("night", night);
        ctx.sun.intensity = night ? 0.08f : 3.0f;
        ctx.sun.color = night ? Vec3{0.6f, 0.7f, 1.0f} : Vec3{1, 1, 1};
        ctx.environment.sky = night ? Vec3{0.02f, 0.03f, 0.06f} : Vec3{0.45f, 0.6f, 0.85f};
        ctx.environment.ambient = night ? 0.15f : 0.35f;
        ctx.environment.exposure = night ? 2.5f : 1.0f;
    }

    void apply_rotation(Context& ctx) {
        Transform* t = ctx.world.get<Transform>(camera_);
        t->rotation = Quat::from_axis_angle({0, 1, 0}, yaw_) * Quat::from_axis_angle({1, 0, 0}, pitch_);
    }

    bool capture_;
    bool night_;
    Entity camera_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
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
    bool night = false;
    std::uint32_t frames = 40, width = 1280, height = 720;
    for (int i = 1; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : "0"; };
        if (!std::strcmp(argv[i], "--capture")) out = next();
        else if (!std::strcmp(argv[i], "--frames")) frames = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--width")) width = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--height")) height = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--night")) night = true;
    }
    App app({.title = "Astral Open World", .width = width, .height = height,
             .fixed_timestep = out ? 1.0f / 60.0f : 0.0f, .asset_root = ASTRAL_OPEN_WORLD_ASSETS,
             .offscreen = out != nullptr});
    OpenWorld game(out != nullptr, night);
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
