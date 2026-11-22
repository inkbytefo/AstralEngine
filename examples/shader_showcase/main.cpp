// Shader showcase (M10, ADR-0022): every surface here is a .material file on a .ashader shader.
//
//   shader_showcase                      fly around: WASD, Q/E, Shift fast, right mouse look, F1 → Materials
//   shader_showcase --capture out.ppm [--frames 60] [--width 1280] [--height 720] [--raster]
//
// Left to right: a leaf plant swaying in the wind (alpha test + vertex()), two glass spheres (blend_mix,
// one a child material of the other), a lava ball (animated noise, lights its surroundings), a hologram
// (blend_add). The sky is a sky shader, the vignette a post shader. Edit any file under assets/ while
// it runs and the picture changes.

#include <astral/astral.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace astral;

namespace {

class Showcase final : public Game {
public:
    explicit Showcase(bool capture) : capture_(capture) {}

    void on_start(Context& ctx) override {
        ctx.sun.direction = normalize(Vec3{-0.45f, -0.28f, -0.75f});
        ctx.sun.color = {1.0f, 0.70f, 0.45f};
        ctx.sun.intensity = 2.0f;
        ctx.environment.sky = {0.16f, 0.18f, 0.40f};
        ctx.environment.ground = {0.05f, 0.04f, 0.05f};
        ctx.environment.ambient = 0.30f;
        ctx.environment.exposure = 0.75f;
        ctx.sky = ctx.load_material("materials/dusk_sky.material");
        ctx.post = {ctx.load_material("materials/vignette.material")};

        // Ground and a backdrop wall (plain PBR: the standard shader is the default).
        ctx.world.create(Transform{.position = {0, 0, 0}, .scale = {40, 1, 40}},
                         PrimitiveMesh{.shape = Primitive::Plane, .material = {.base_color = {0.10f, 0.09f, 0.09f, 1}, .roughness = 0.85f}});
        ctx.world.create(Transform{.position = {0, 2.5f, -3.2f}, .scale = {16, 5, 0.3f}},
                         PrimitiveMesh{.shape = Primitive::Cube, .material = {.base_color = {0.11f, 0.12f, 0.15f, 1}, .roughness = 0.9f}});
        // Stripes behind the glass so the transparency reads.
        for (int i = 0; i < 4; ++i) {
            ctx.world.create(Transform{.position = {-2.8f + static_cast<float>(i) * 1.1f, 1.4f, -2.9f}, .scale = {0.35f, 2.8f, 0.2f}},
                             PrimitiveMesh{.shape = Primitive::Cube,
                                           .material = {.base_color = i % 2 ? Vec4{0.9f, 0.25f, 0.2f, 1} : Vec4{0.95f, 0.85f, 0.3f, 1}, .roughness = 0.7f}});
        }

        // Leaves: upright planes leaning out of one stem, each turned a little around it.
        for (int i = 0; i < 5; ++i) {
            const float k = static_cast<float>(i) - 2.0f;
            const float lean = k * 0.30f, yaw = k * 0.42f;
            const Quat turn = Quat::from_axis_angle({0, 1, 0}, yaw);
            const Quat rotation = turn * Quat::from_axis_angle({0, 0, 1}, lean) * Quat::from_axis_angle({1, 0, 0}, radians(90.0f));
            const Vec3 up = turn * Vec3{-std::sin(lean), std::cos(lean), 0.0f};
            ctx.world.create(Transform{.position = {-5.2f + up.x * 1.05f, 0.25f + up.y * 1.05f, up.z * 1.05f}, .rotation = rotation,
                                       .scale = {0.85f, 1.0f, 2.1f}},
                             PrimitiveMesh{.shape = Primitive::Plane, .material_file = "materials/leaf.material"});
        }
        ctx.world.create(Transform{.position = {-5.2f, 0.2f, 0.0f}, .scale = {0.12f, 0.4f, 0.12f}},
                         PrimitiveMesh{.shape = Primitive::Cube, .material = {.base_color = {0.18f, 0.25f, 0.08f, 1}}});

        ctx.world.create(Transform{.position = {-2.0f, 1.3f, 0.0f}, .scale = {2.2f, 2.2f, 2.2f}},
                         PrimitiveMesh{.shape = Primitive::Sphere, .material_file = "materials/glass.material"});
        ctx.world.create(Transform{.position = {0.45f, 1.0f, 0.6f}, .scale = {1.7f, 1.7f, 1.7f}},
                         PrimitiveMesh{.shape = Primitive::Sphere, .material_file = "materials/glass_blue.material"});

        ctx.world.create(Transform{.position = {2.9f, 1.0f, 0.2f}, .scale = {2.0f, 2.0f, 2.0f}},
                         PrimitiveMesh{.shape = Primitive::Sphere, .material_file = "materials/lava.material"});
        ctx.world.create(Transform{.position = {2.9f, 2.0f, 1.2f}}, PointLight{.color = {1.0f, 0.5f, 0.15f}, .intensity = 40.0f, .radius = 9.0f});

        ctx.world.create(Transform{.position = {5.4f, 1.5f, 0.0f}, .scale = {1.0f, 3.0f, 1.0f}},
                         PrimitiveMesh{.shape = Primitive::Cube, .material_file = "materials/hologram.material"});

        camera_ = ctx.world.create(Transform{.position = {0.0f, 2.0f, 7.6f},
                                             .rotation = Quat::from_axis_angle({1, 0, 0}, radians(-7.0f))},
                                   Camera{.far_plane = 200.0f});
    }

    void on_update(Context& ctx) override {
        if (capture_ || !camera_.valid()) return;
        const Input& in = ctx.input;
        if (in.pressed(Key::Escape)) ctx.quit();
        if (in.down(MouseButton::Right)) {
            yaw_ -= in.mouse_delta().x * 0.003f;
            pitch_ = std::fmax(-1.4f, std::fmin(1.4f, pitch_ - in.mouse_delta().y * 0.003f));
        }
        Vec3 move{(in.down(Key::D) ? 1.0f : 0.0f) - (in.down(Key::A) ? 1.0f : 0.0f),
                  (in.down(Key::E) ? 1.0f : 0.0f) - (in.down(Key::Q) ? 1.0f : 0.0f),
                  (in.down(Key::S) ? 1.0f : 0.0f) - (in.down(Key::W) ? 1.0f : 0.0f)};
        Transform* t = ctx.world.get<Transform>(camera_);
        t->rotation = Quat::from_axis_angle({0, 1, 0}, yaw_) * Quat::from_axis_angle({1, 0, 0}, pitch_);
        const Vec3 step = t->rotation * move;
        const float speed = (in.down(Key::LeftShift) ? 12.0f : 4.0f) * ctx.time.delta;
        t->position = {t->position.x + step.x * speed, t->position.y + step.y * speed, t->position.z + step.z * speed};
    }

private:
    bool capture_;
    Entity camera_{};
    float yaw_ = 0.0f;
    float pitch_ = radians(-7.0f);
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
    std::uint32_t frames = 60, width = 1280, height = 720;
    bool raster = false;
    for (int i = 1; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : "0"; };
        if (!std::strcmp(argv[i], "--capture")) out = next();
        else if (!std::strcmp(argv[i], "--frames")) frames = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--width")) width = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--height")) height = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--raster")) raster = true; // no ray tracing: the raster-only look
    }
    RenderSettings render;
    render.bloom_strength = 0.06f;
    render.ray_tracing = !raster;
    App app({.title = "Astral Shader Showcase", .width = width, .height = height,
             .fixed_timestep = out ? 1.0f / 60.0f : 0.0f, .render = render, .asset_root = ASTRAL_GAME_ASSETS,
             .pack = ASTRAL_GAME_PACK, .offscreen = out != nullptr});
    Showcase game(out != nullptr);
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
