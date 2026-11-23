#include <astral/astral.hpp>
#include <doctest/doctest.h>
#include <imgui.h> // games get Dear ImGui through Astral::App

#include <string>
#include <thread>

using namespace astral;

namespace devtools_test {
class SlowSystem final : public System {
public:
    void update(Context& ctx) override {
        ASTRAL_PROFILE_SCOPE(ctx.profiler, "custom scope");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
};
} // namespace devtools_test

TEST_CASE("the App profiles its phases and every system by name") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f});
    struct G final : Game {
        void on_start(Context& ctx) override {
            const auto& s = ctx.add_system<devtools_test::SlowSystem>(Phase::Update);
            name = s.name();
        }
        std::string name;
    } game;
    app.run_frames(game, 3);
    CHECK(game.name == "devtools_test::SlowSystem");
    const Profiler& p = *app.context().profiler;
    CHECK(p.frame_times().size() == 3);
    bool phase = false, system = false, custom = false, physics = false;
    std::uint16_t system_depth = 0, custom_depth = 0;
    for (const ProfileSample& s : p.last_frame()) {
        const std::string_view n = s.name;
        phase |= n == "Update";
        physics |= n == "physics.step";
        if (n == "devtools_test::SlowSystem") {
            system = true;
            system_depth = s.depth;
            CHECK(s.ms >= 0.9f);
        }
        if (n == "custom scope") {
            custom = true;
            custom_depth = s.depth;
        }
    }
    CHECK(phase);
    CHECK(physics);
    CHECK(system);
    CHECK(custom);
    CHECK(system_depth == 1);       // inside "Update"
    CHECK(custom_depth == 2);       // inside the system
    CHECK(p.scope_ms("Update") >= p.scope_ms("devtools_test::SlowSystem"));
}

TEST_CASE("ctx.debug: one-frame and timed shapes; overlays draw in headless mode too") {
    App app({.headless = true, .fixed_timestep = 0.1f});
    struct G final : Game {
        std::vector<std::size_t> seen;
        void on_start(Context& ctx) override {
            ctx.debug_show.physics = true;
            ctx.debug_show.lights = true;
            ctx.world.create(Transform{}, RigidBody{.desc = {.shape = ShapeDesc::capsule(0.3f, 0.5f)}});
            ctx.world.create(Transform{}, PointLight{});
            ctx.debug.line({0, 0, 0}, {1, 0, 0}, {1, 1, 1, 1}, {.seconds = 0.25f}); // three frames of 0.1 s
        }
        void on_update(Context& ctx) override {
            seen.push_back(ctx.debug.lines().size()); // what is left from earlier frames
            ctx.debug.cross({0, 0, 0}, 1, {1, 0, 0, 1}); // this frame only
        }
    } game;
    app.run_frames(game, 5);
    REQUIRE(game.seen.size() == 5);
    CHECK(game.seen[0] == 1); // the timed line
    CHECK(game.seen[1] == 1);
    CHECK(game.seen[2] == 1);
    CHECK(game.seen[3] == 0); // 0.25 s are over; one-frame shapes never carry over
    CHECK(game.seen[4] == 0);
}

TEST_CASE("the debug UI draws the engine's windows and the game's own panel over the image") {
    struct G final : Game {
        bool ui = false;
        bool game_panel_built = false;
        void on_start(Context& ctx) override {
            ctx.show_debug_ui = ui;
            ctx.world.create(Name{"some entity"}, Transform{});
        }
        void on_debug_ui(Context&) override {
            ImGui::Begin("Game panel");
            ImGui::Text("score: 3");
            ImGui::End();
            game_panel_built = true;
        }
    };
    std::vector<std::uint8_t> with_ui, without_ui;
    bool panel = false;
    for (const bool ui : {true, false}) {
        App app({.width = 320, .height = 200, .fixed_timestep = 1.0f / 60.0f, .offscreen = true});
        if (!app.context().renderer) return; // no GPU
        G game;
        game.ui = ui;
        app.run_frames(game, 3);
        REQUIRE(app.read_frame(ui ? with_ui : without_ui));
        if (ui) panel = game.game_panel_built;
        else CHECK_FALSE(game.game_panel_built); // closed UI: no ImGui work at all
    }
    CHECK(panel);
    // The menu bar covers the top rows: they differ from the plain image.
    std::size_t different = 0;
    for (std::size_t x = 0; x < 320; ++x) {
        const std::size_t i = (5 * 320 + x) * 4;
        different += std::abs(int(with_ui[i]) - int(without_ui[i])) > 8 ? 1 : 0;
    }
    CHECK(different > 200);
}

TEST_CASE("ctx.hud lays out text every frame (headless too) and starts empty each frame") {
    App app({.width = 400, .height = 300, .headless = true, .fixed_timestep = 1.0f / 60.0f});
    struct G final : Game {
        std::vector<std::size_t> commands_at_start;
        Vec2 size{}, screen{};
        void on_update(Context& ctx) override {
            commands_at_start.push_back(ctx.hud.layer().commands.size());
            REQUIRE(ctx.font);
            screen = ctx.screen_size();
            size = ctx.hud.text(*ctx.font, "Skor 3 : 1 — İyi oyun!", {screen.x * 0.5f, 10}, {.size = 32, .align = TextAlign::Center, .shadow = true});
            ctx.hud.rect({0, 0}, {50, 50}, {0, 0, 0, 0.5f});
        }
    } game;
    app.run_frames(game, 3);
    CHECK(game.commands_at_start == std::vector<std::size_t>{0, 0, 0});
    CHECK(game.size.x > 200.0f);
    CHECK(game.screen.x == 400.0f);
}

TEST_CASE("ViewportService renders 3D scene into offscreen texture sampled by ImGui and handles resize") {
    App app({.width = 320, .height = 200, .fixed_timestep = 1.0f / 60.0f, .offscreen = true,
             .debug_ui = true, .debug_ui_builtin_panels = false});
    if (!app.context().renderer) return; // no GPU
    struct G final : Game {
        int frame = 0;
        TextureHandle last_tex{};
        gpu::Extent2D last_size{};
        void on_start(Context& ctx) override {
            ctx.show_debug_ui = true;
            ctx.world.create(Transform{.position = {0, 0, 5}}, Camera{.active = true});
            ctx.world.create(Transform{}, PrimitiveMesh{.shape = Primitive::Cube,
                                                        .material = {.base_color = {0.9f, 0.2f, 0.2f, 1.0f}}});
        }
        void on_debug_ui(Context& ctx) override {
            const std::uint32_t w = frame < 2 ? 160u : 220u;
            const std::uint32_t h = frame < 2 ? 120u : 140u;
            ctx.viewport().set_size(w, h);
            last_tex = ctx.viewport().texture();
            last_size = ctx.viewport().size();
            ImGui::SetNextWindowPos(ImVec2(10, 10));
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w) + 16, static_cast<float>(h) + 36));
            if (ImGui::Begin("Viewport")) {
                if (last_tex.valid()) {
                    ImGui::Image(static_cast<ImTextureID>(pack_ui_texture(last_tex)),
                                 ImVec2(static_cast<float>(w), static_cast<float>(h)));
                }
            }
            ImGui::End();
            ++frame;
        }
    } game;
    app.run_frames(game, 4);
    CHECK(game.last_tex.valid());
    CHECK(game.last_size.width == 220u);
    CHECK(game.last_size.height == 140u);
    std::vector<std::uint8_t> rgba;
    REQUIRE(app.read_frame(rgba));
    CHECK(rgba.size() == 320u * 200u * 4u);
    // Verify non-black rendered pixels inside the viewport region (red cube / blue sky).
    std::size_t lit = 0;
    for (std::uint32_t y = 40; y < 140; ++y) {
        for (std::uint32_t x = 20; x < 200; ++x) {
            const std::size_t i = (std::size_t{y} * 320u + x) * 4u;
            if (rgba[i] > 35 || rgba[i + 2] > 35) ++lit;
        }
    }
    CHECK(lit > 500);
}
