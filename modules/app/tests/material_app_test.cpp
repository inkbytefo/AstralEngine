#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <bit>
#include <chrono>
#include <thread>

using namespace astral;

namespace {

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
    // Edits within one filesystem tick would look unchanged to the hot-reload poll: every write gets a new time.
    static int edits = 0;
    std::filesystem::last_write_time(file, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(++edits * 10));
}

constexpr std::string_view solid_shader = R"(shader_type surface;
uniform vec3 color : source_color = vec3(1.0, 1.0, 1.0);
void surface() { ALBEDO = vec3(0.0); EMISSION = color * 2.0; ROUGHNESS = 1.0; }
)";

std::string material(const char* parent, const char* color) {
    std::string s = R"({"format":"astral.material","version":1,)";
    if (parent) s += std::string(R"("parent":")") + parent + R"(",)";
    else s += R"("shader":"shaders/solid.ashader",)";
    return s + R"("values":{"color":)" + color + "}}";
}

struct Rgb {
    int r, g, b;
};
Rgb center(const std::vector<std::uint8_t>& rgba, std::uint32_t w, std::uint32_t h) {
    const std::size_t i = (std::size_t{h / 2} * w + w / 2) * 4;
    return {rgba[i], rgba[i + 1], rgba[i + 2]};
}

} // namespace

TEST_CASE("a .material file draws, follows edits while the game runs and survives a broken edit") {
    const auto dir = test::temp_dir("app_materials");
    write_text(dir / "shaders" / "solid.ashader", solid_shader);
    write_text(dir / "materials" / "base.material", material(nullptr, "[1, 0, 0]"));
    write_text(dir / "materials" / "child.material", material("materials/base.material", "[0, 1, 0]"));

    constexpr std::uint32_t w = 160, h = 120;
    App app({.width = w, .height = h, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .offscreen = true});
    if (!app.context().renderer) return; // no GPU

    // One run; the files are edited from inside it (an App runs once). Edits are 40 frames apart, the
    // hot-reload poll looks every 0.25 s = 15 frames.
    struct G final : Game {
        const std::filesystem::path& dir;
        MaterialHandle red{}, green{};
        ShaderHandle shader_at_start{}, shader_after_break{}, shader_after_fix{};
        std::vector<float> params_start, params_edited, params_after_break;
        explicit G(const std::filesystem::path& d) : dir(d) {}
        static std::vector<float> params(Context& ctx, MaterialHandle m) {
            const MaterialDesc* d = ctx.renderer->material(m);
            std::vector<float> out;
            if (d) {
                for (std::size_t i = 0; i < 3 && i < d->params.size(); ++i) out.push_back(std::bit_cast<float>(d->params[i])); // float words (ADR-0042)
            }
            return out;
        }
        void on_start(Context& ctx) override {
            red = ctx.materials().load_material("materials/base.material");
            green = ctx.materials().load_material("materials/child.material");
            ctx.world.create(Transform{.position = {0, 0, 0}, .scale = {3, 3, 3}},
                             PrimitiveMesh{.shape = Primitive::Cube, .material_file = "materials/child.material"});
            ctx.world.create(Transform{.position = {0, 0, 6}}, Camera{});
            ctx.sun.intensity = 0.0f;
            ctx.environment.ambient = 0.0f;
        }
        void on_update(Context& ctx) override {
            switch (ctx.time.frame) {
            case 3:
                params_start = params(ctx, green);
                shader_at_start = ctx.renderer->material(green)->shader;
                write_text(dir / "materials" / "child.material", material("materials/base.material", "[0, 0, 1]"));
                break;
            case 50:
                params_edited = params(ctx, green);
                write_text(dir / "shaders" / "solid.ashader", "shader_type surface;\nvoid surface() { ALBEDO = vec3(oops); }\n");
                break;
            case 90:
                params_after_break = params(ctx, green);
                shader_after_break = ctx.renderer->material(green)->shader;
                write_text(dir / "shaders" / "solid.ashader",
                           "shader_type surface;\nuniform vec3 color = vec3(1.0);\n"
                           "void surface() { ALBEDO = vec3(0.0); EMISSION = color.bgr * 2.0; }\n");
                break;
            case 130:
                shader_after_fix = ctx.renderer->material(green)->shader;
                break;
            default: break;
            }
        }
    };
    G game(dir);
    app.run_frames(game, 140);

    CHECK(game.red.valid());
    CHECK(game.green.valid());
    CHECK(game.red != game.green);
    REQUIRE(game.params_start.size() == 3);
    CHECK(game.params_start == std::vector<float>{0, 1, 0});   // the child's value over the parent's
    CHECK(game.params_edited == std::vector<float>{0, 0, 1});  // edited file, same handle
    CHECK(game.params_after_break == std::vector<float>{0, 0, 1});
    CHECK(game.shader_after_break == game.shader_at_start);    // broken shader: last good pipelines stay
    CHECK(game.shader_after_fix != game.shader_at_start);      // fixed shader: new pipelines
    std::vector<std::uint8_t> frame;
    REQUIRE(app.read_frame(frame));
    const Rgb c = center(frame, w, h);
    CHECK(c.r > 150); // blue value, swapped to red by the fixed shader
    CHECK(c.b < 60);
}

TEST_CASE("load_material without a renderer or with a missing file gives an invalid handle") {
    App headless({.headless = true});
    struct G final : Game {
        MaterialHandle m{};
        void on_start(Context& ctx) override { m = ctx.materials().load_material("nothing.material"); }
    } game;
    headless.run_frames(game, 1);
    CHECK_FALSE(game.m.valid());
}

TEST_CASE("a material recovers a missing texture and preserves it across broken edits") {
    const auto dir = test::temp_dir("app_material_texture_reload");
    const auto texture = dir / "color.png";
    write_text(dir / "textured.ashader", R"(shader_type surface;
uniform sampler2D albedo : source_color;
void surface() { ALBEDO = vec3(0.0); EMISSION = texture(albedo, UV).rgb; }
)");
    write_text(dir / "wall.material", R"({"format":"astral.material","version":1,
        "shader":"textured.ashader","values":{"albedo":"color.png"}})");
    App app({.width = 80, .height = 60, .fixed_timestep = 1.0f / 60.0f, .render = {.ray_tracing = false},
             .asset_root = dir.string(), .offscreen = true});
    if (!app.context().renderer) return;

    struct G final : Game {
        std::filesystem::path path;
        MaterialHandle material;
        TextureHandle missing, loaded, after_bad, after_edit;
        TextureHandle bound(Context& ctx) const { return ctx.renderer->material(material)->textures[0].texture; }
        void on_start(Context& ctx) override {
            material = ctx.materials().load_material("wall.material");
            REQUIRE(material.valid());
            missing = bound(ctx);
            ctx.world.create(Transform{}, PrimitiveMesh{.material_file = "wall.material"});
            ctx.world.create(Transform{.position = {0, 0, 3}}, Camera{});
        }
        void on_update(Context& ctx) override {
            if (ctx.time.frame == 2) test::write_bytes(path, test::png(255, 0, 0));
            if (ctx.time.frame == 20) {
                loaded = bound(ctx);
                write_text(path, "invalid image");
            }
            if (ctx.time.frame == 37) {
                after_bad = bound(ctx);
                test::write_bytes(path, test::png(0, 255, 0));
            }
            if (ctx.time.frame == 54) after_edit = bound(ctx);
        }
    } game;
    game.path = texture;
    app.run_frames(game, 57);
    CHECK_FALSE(game.missing.valid());
    CHECK(game.loaded.valid());
    CHECK(game.after_bad == game.loaded);
    CHECK(game.after_edit == game.loaded);
    std::vector<std::uint8_t> frame;
    REQUIRE(app.read_frame(frame));
    CHECK(center(frame, 80, 60).g > 150);
    CHECK(center(frame, 80, 60).r < 30);
}

TEST_CASE("standard PBR material assets inherit values and draw a primitive") {
    const auto dir = test::temp_dir("app_pbr_material");
    write_text(dir / "plaster.material", R"({"format":"astral.material","version":1,"type":"pbr",
        "values":{"roughness":0.85,"base_color_texture":"plaster.png"}})");
    write_text(dir / "green.material", R"({"format":"astral.material","version":1,"parent":"plaster.material",
        "values":{"emissive":[0,1,0]}})");
    test::write_bytes(dir / "plaster.png", test::png(205, 200, 185));
    App app({.width = 80, .height = 60, .render = {.ray_tracing = false},
             .asset_root = dir.string(), .offscreen = true});
    if (!app.context().renderer) return;
    struct G final : Game {
        void on_start(Context& ctx) override {
            const auto handle = ctx.materials().load_material("green.material");
            REQUIRE(handle.valid());
            const auto* desc = ctx.renderer->material(handle);
            REQUIRE(desc);
            CHECK_FALSE(desc->shader.valid());
            CHECK(desc->roughness == doctest::Approx(0.85f));
            CHECK(desc->base_color_texture.valid());
            ctx.world.create(Transform{}, PrimitiveMesh{.material_file = "green.material"});
            ctx.world.create(Transform{.position = {0, 0, 3}}, Camera{});
            ctx.sun.intensity = 0.0f;
            ctx.environment.ambient = 0.0f;
        }
    } game;
    app.run_frames(game, 2);
    std::vector<std::uint8_t> frame;
    REQUIRE(app.read_frame(frame));
    const Rgb c = center(frame, 80, 60);
    CHECK(c.g > 150);
    CHECK(c.r < 30);
}
