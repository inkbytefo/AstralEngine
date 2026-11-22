#include "frame/frame_slot.hpp"
#include "frame/instances.hpp"
#include "gpu_test_support.hpp"
#include "passes/bloom_pass.hpp"
#include "passes/ddgi_pass.hpp"
#include "passes/denoise_pass.hpp"
#include "passes/post_pass.hpp"
#include "passes/rt_lighting_pass.hpp"
#include "passes/shadow_pass.hpp"
#include "passes/taa_pass.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/render/mesh.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/surface_shader.hpp>

#include <array>
#include <cstdint>

using namespace astral;

TEST_CASE("ShadowPass prepares sun bounds, disables cleanly when empty or off, and resizes") {
    test::TestDevice device;
    if (!device) return;
    MeshStore meshes(*device);
    ShadowPass shadow(*device, 256);

    const ShadowFrame empty = shadow.prepare({}, meshes);
    CHECK_FALSE(empty.enabled);
    CHECK(empty.bindless_index == no_shadow);

    const MeshHandle cube = meshes.create(make_cube(), false);
    const std::array items{MeshInstance{.mesh = cube, .transform = translate({0, 1, 0})}};
    const ShadowFrame disabled = shadow.prepare({.meshes = items, .sun = {.shadows = false}}, meshes);
    CHECK_FALSE(disabled.enabled);
    CHECK(disabled.bindless_index == no_shadow);

    const ShadowFrame active = shadow.prepare({.meshes = items, .sun = {.shadows = true}}, meshes);
    CHECK(active.enabled);
    CHECK(active.texel_world > 0.0f);
    CHECK(active.bindless_index != no_shadow);

    shadow.resize(512);
    const ShadowFrame finer = shadow.prepare({.meshes = items, .sun = {.shadows = true}}, meshes);
    CHECK(finer.enabled);
    CHECK(finer.texel_world == doctest::Approx(active.texel_world * 0.5f).epsilon(0.01));
}

TEST_CASE("DdgiPass fits scene volume, preserves valid history when stationary, and resets on resize or movement") {
    test::TestDevice device;
    if (!device) return;
    MeshStore meshes(*device);
    DdgiPass ddgi(*device);

    const MeshHandle cube = meshes.create(make_cube(), false);
    const std::array items{MeshInstance{.mesh = cube, .transform = translate({0, 1, 0}) * scale({4, 2, 4})}};
    const RenderScene scene{.meshes = items};

    GiSettings gi{.probes_x = 4, .probes_y = 3, .probes_z = 4, .rays_per_probe = 32};
    ddgi.ensure(scene, meshes, gi);
    CHECK(ddgi.probe_count() == 48u);
    CHECK_FALSE(ddgi.info().valid);
    CHECK(ddgi.info().spacing.x == doctest::Approx(1.0f));
    CHECK(ddgi.info().spacing.y == doctest::Approx(2.0f / 3.0f));

    ddgi.mark_valid();
    ddgi.ensure(scene, meshes, gi);
    CHECK(ddgi.info().valid);

    // Moving the explicit volume invalidates accumulated probes.
    gi.volume_min = {10, 0, 0};
    gi.volume_max = {14, 2, 4};
    ddgi.ensure(scene, meshes, gi);
    CHECK_FALSE(ddgi.info().valid);

    ddgi.mark_valid();
    ddgi.reset();
    CHECK_FALSE(ddgi.info().valid);

    // Changing grid resolution reallocates probe atlases and invalidates history.
    ddgi.mark_valid();
    gi.probes_x = 6;
    ddgi.ensure(scene, meshes, gi);
    CHECK(ddgi.probe_count() == 72u);
    CHECK_FALSE(ddgi.info().valid);
}

TEST_CASE("temporal history passes report resize transitions accurately") {
    test::TestDevice device;
    if (!device) return;
    RtLightingPass rt_lighting(*device);
    DenoisePass denoise(*device);
    TaaPass taa(*device);

    CHECK(rt_lighting.resize(32, 32));
    CHECK_FALSE(rt_lighting.resize(32, 32));
    CHECK(rt_lighting.resize(64, 32));

    CHECK(denoise.resize(32, 32));
    CHECK_FALSE(denoise.resize(32, 32));
    CHECK(denoise.resize(64, 32));

    CHECK(taa.resize(32, 32));
    CHECK_FALSE(taa.resize(32, 32));
    CHECK(taa.resize(64, 32));
}

TEST_CASE("PostPass filters non-post materials and BloomPass skips when strength is zero") {
    test::TestDevice device;
    if (!device) return;
    TextureStore textures(*device);
    ShaderStore shaders(*device, gpu::Format::RGBA8Unorm);
    MaterialStore materials(*device, textures, shaders);
    MeshStore meshes(*device);

    const ShaderParse post_parse = parse_ashader("shader_type post;\nvoid post() { COLOR = COLOR.bgr; }\n");
    REQUIRE(post_parse.errors.empty());
    const ScreenShaderBuild post_build = compile_screen_shader(post_parse.source, "pass_unit_post");
    REQUIRE(post_build.ok());
    const ShaderHandle post_shader = shaders.create(post_build, "pass_unit_post");
    REQUIRE(post_shader.valid());

    const MaterialHandle plain = materials.create({.base_color = {1, 1, 1, 1}});
    const MaterialHandle post_mat = materials.create({.shader = post_shader});
    const std::array chain_in{plain, MaterialHandle{}, post_mat};

    PostPass post;
    const auto filtered = post.collect({.post = chain_in}, materials);
    REQUIRE(filtered.size() == 1);
    CHECK(filtered[0] == post_mat);

    BloomPass bloom(*device);
    RenderGraph graph(*device);
    graph.begin_frame();
    const RGTexture src = graph.create("src", {32, 32, gpu::Format::RGBA16Float});
    CameraData cam;
    RenderScene scene;
    RenderSettings settings{.bloom_strength = 0.0f};
    FrameInstances instances;
    FrameSlot slot;
    const FrameInputs inputs{
        .camera = cam,
        .scene = scene,
        .settings = settings,
        .meshes = meshes,
        .materials = materials,
        .shaders = shaders,
        .textures = textures,
        .instances = instances,
        .slot = slot,
        .extent = {32, 32},
    };
    CHECK_FALSE(bloom.add(graph, inputs, src).valid());

    slot.destroy(*device);
    shaders.destroy(post_shader);
}
