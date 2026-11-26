#include "gpu_test_support.hpp"

#include <astral/render/renderer.hpp>

#include <doctest/doctest.h>

#include <memory>

using namespace astral;

// A window that is loading assets still has to answer the OS ("not responding" after a few seconds): the App
// pumps its events from this callback, which the Renderer calls after every resource it creates or updates.
TEST_CASE("the renderer reports progress after each resource it creates") {
    test::TestDevice device;
    if (!device) return;
    Renderer renderer(*device, gpu::Format::RGBA8Unorm, {.shadow_map_size = 256, .ray_tracing = false});
    int calls = 0;
    renderer.set_busy_callback([&] { ++calls; });

    const TextureHandle texture = renderer.create_texture({.width = 2, .height = 2, .rgba = std::vector<std::uint8_t>(16, 255)});
    REQUIRE(texture.valid());
    CHECK(calls == 1);
    CHECK(renderer.update_texture(texture, {.width = 2, .height = 2, .rgba = std::vector<std::uint8_t>(16, 128)}));
    CHECK(calls == 2);
    REQUIRE(renderer.create_mesh(make_cube()).valid());
    CHECK(calls == 3);
    const MaterialHandle material = renderer.create_material({});
    REQUIRE(material.valid());
    CHECK(calls == 4);
    CHECK(renderer.update_material(material, {.roughness = 0.2f}));
    CHECK(calls == 5); // updates count too, like update_texture

    const ShaderParse parsed = parse_ashader("shader_type surface;\nvoid surface() { ALBEDO = vec3(0.5); }\n");
    REQUIRE(parsed.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(parsed.source, "busy.ashader");
    REQUIRE(build.ok());
    REQUIRE(renderer.create_shader(build, "busy").valid());
    CHECK(calls == 6);

    renderer.set_busy_callback({}); // removing it stops the calls
    renderer.create_material({});
    CHECK(calls == 6);
}
