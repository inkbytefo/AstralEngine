#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "features/fly_camera/fly_camera.hpp"

#include <astral/render/surface_shader.hpp>
#include <astral/world/scene.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the showcase loads every exhibit and its authored camera and lighting") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    shader_showcase::ShowcaseGame game;
    app.run_frames(game, 3);

    Context& ctx = app.context();
    World& world = ctx.world;
    CHECK_FALSE(ctx.quit_requested());
    CHECK(ctx.scripts->take_errors().empty());

    struct Exhibit {
        std::string_view name;
        Primitive shape;
        std::string_view material;
    };
    constexpr std::array exhibits{
        Exhibit{"leaf_1", Primitive::Plane, "materials/leaf.material"},
        Exhibit{"leaf_2", Primitive::Plane, "materials/leaf.material"},
        Exhibit{"leaf_3", Primitive::Plane, "materials/leaf.material"},
        Exhibit{"leaf_4", Primitive::Plane, "materials/leaf.material"},
        Exhibit{"leaf_5", Primitive::Plane, "materials/leaf.material"},
        Exhibit{"leaf_6", Primitive::Plane, "materials/leaf.material"},
        Exhibit{"leaf_7", Primitive::Plane, "materials/leaf.material"},
        Exhibit{"glass", Primitive::Sphere, "materials/glass.material"},
        Exhibit{"blue_glass", Primitive::Sphere, "materials/glass_blue.material"},
        Exhibit{"lava", Primitive::Sphere, "materials/lava.material"},
        Exhibit{"alloy", Primitive::Sphere, "materials/alloy.material"},
        Exhibit{"hologram", Primitive::Cube, "materials/hologram.material"},
        Exhibit{"plasma_shell", Primitive::Sphere, "materials/plasma_shell.material"},
        Exhibit{"floor", Primitive::Plane, "materials/gallery_floor.material"},
    };
    CHECK(world.count<PrimitiveMesh>() == 31);
    for (const Exhibit& exhibit : exhibits) {
        CAPTURE(exhibit.name);
        const PrimitiveMesh* mesh = world.get<PrimitiveMesh>(find_by_name(world, exhibit.name));
        REQUIRE(mesh != nullptr);
        CHECK(mesh->shape == exhibit.shape);
        CHECK(mesh->material_file == exhibit.material);
    }

    CHECK(world.count<PointLight>() == 5);
    const PointLight* light = world.get<PointLight>(find_by_name(world, "lava_light"));
    REQUIRE(light != nullptr);
    CHECK(light->intensity == doctest::Approx(20.0f));
    CHECK(ctx.sun.intensity == doctest::Approx(0.95f));
    CHECK(ctx.environment.exposure == doctest::Approx(0.86f));

    CHECK(world.count<Camera>() == 1);
    CHECK(world.count<fly::FlyCamera>() == 1);
    const Entity camera_entity = find_by_name(world, "camera");
    REQUIRE(camera_entity.valid());
    const Camera* camera = world.get<Camera>(camera_entity);
    const Transform* transform = world.get<Transform>(camera_entity);
    const fly::FlyCamera* fly_camera = world.get<fly::FlyCamera>(camera_entity);
    REQUIRE(camera != nullptr);
    REQUIRE(transform != nullptr);
    REQUIRE(fly_camera != nullptr);
    CHECK(camera->active);
    CHECK(camera->far_plane == doctest::Approx(100.0f));
    CHECK(transform->position.z == doctest::Approx(9.82f));
    CHECK(fly_camera->speed == doctest::Approx(4.0f));
    CHECK(fly_camera->fast_speed == doctest::Approx(12.0f));
    CHECK(transform->rotation.x == doctest::Approx(-0.05233605f));
}

TEST_CASE("the showcase shaders compile for their draw paths and together for ray tracing") {
    constexpr std::array shaders{
        "foliage", "glass", "lava", "hologram", "dusk_sky", "vignette",
        "alloy", "gallery_floor", "plasma_shell",
    };
    std::vector<RtSurface> rt_surfaces;
    for (const std::string_view name : shaders) {
        CAPTURE(name);
        const auto path = std::filesystem::path(ASTRAL_GAME_ASSETS) / "shaders" / (std::string(name) + ".ashader");
        std::ifstream file(path, std::ios::binary);
        REQUIRE(file.is_open());
        const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        const ShaderParse parsed = parse_ashader(source);
        for (const auto& error : parsed.errors) FAIL_CHECK(error.line, ": ", error.message);
        REQUIRE(parsed.errors.empty());

        if (parsed.source.type == ShaderType::Surface) {
            auto shader = compile_surface_shader(parsed.source, name);
            for (const auto& error : shader.errors) FAIL_CHECK(error.line, ": ", error.message);
            REQUIRE(shader.ok());
            if (!shader.rt_chunk.empty()) {
                rt_surfaces.push_back({static_cast<std::uint32_t>(rt_surfaces.size()), std::move(shader.rt_chunk)});
            }
        } else {
            const auto shader = compile_screen_shader(parsed.source, name);
            for (const auto& error : shader.errors) FAIL_CHECK(error.line, ": ", error.message);
            REQUIRE(shader.ok());
        }
    }
    REQUIRE(rt_surfaces.size() == 4);
    const auto rt = compile_rt_variants(rt_surfaces);
    INFO(rt.log);
    CHECK(rt.ok());
}
