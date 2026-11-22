#include "frame/frame_slot.hpp"
#include "frame/instances.hpp"
#include "gpu_layouts.hpp"
#include "gpu_test_support.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/render/mesh.hpp>
#include <astral/render/surface_shader.hpp>

#include <array>
#include <cstdint>
#include <vector>

using namespace astral;

TEST_CASE("build_mips generates a full box-filtered mip chain without GPU") {
    ImageData img{.width = 4, .height = 2, .rgba = {}, .srgb = false};
    img.rgba.resize(4 * 2 * 4, 0);
    for (std::size_t i = 0; i < 4 * 2; ++i) {
        img.rgba[i * 4 + 0] = 200;
        img.rgba[i * 4 + 1] = 100;
        img.rgba[i * 4 + 2] = 50;
        img.rgba[i * 4 + 3] = 255;
    }
    const auto mips = build_mips(img);
    REQUIRE(mips.size() == 3); // 4x2 -> 2x1 -> 1x1
    CHECK(mips[0].size() == 4 * 2 * 4);
    CHECK(mips[1].size() == 2 * 1 * 4);
    CHECK(mips[2].size() == 1 * 1 * 4);
    CHECK(mips[2][0] == 200);
    CHECK(mips[2][1] == 100);
    CHECK(mips[2][2] == 50);
    CHECK(mips[2][3] == 255);
}

TEST_CASE("MeshStore creates, updates, destroys meshes and computes local AABB") {
    test::TestDevice device;
    if (!device) return;
    MeshStore store(*device);

    CHECK_FALSE(store.create(MeshData{}, false).valid());
    const MeshData cube = make_cube();
    const MeshHandle h = store.create(cube, false);
    REQUIRE(h.valid());
    const GpuMesh* m = store.get(h);
    REQUIRE(m != nullptr);
    CHECK(m->index_count == cube.indices.size());
    CHECK(m->bounds_min.x == doctest::Approx(-0.5f));
    CHECK(m->bounds_max.x == doctest::Approx(0.5f));
    CHECK_FALSE(m->blas.valid());

    MeshData wide = cube;
    for (Vertex& v : wide.vertices) v.position.x *= 4.0f;
    CHECK(store.update(h, wide, false));
    CHECK(store.get(h)->bounds_min.x == doctest::Approx(-2.0f));
    CHECK(store.get(h)->bounds_max.x == doctest::Approx(2.0f));
    CHECK_FALSE(store.update(MeshHandle{}, wide, false));

    if (device->supports(gpu::Feature::RayTracing)) {
        store.ensure_blas_all();
        CHECK(store.get(h)->blas.valid());
    }

    store.destroy(h);
    CHECK(store.get(h) == nullptr);
}

TEST_CASE("TextureStore provides stand-ins and manages texture lifecycle") {
    test::TestDevice device;
    if (!device) return;
    TextureStore store(*device);

    CHECK(store.white().valid());
    CHECK(store.flat_normal().valid());
    CHECK(store.supports(ImageFormat::RGBA8));
    CHECK(store.bindless_or(TextureHandle{}, no_texture) == no_texture);
    CHECK(store.stand_in_bindless(false) != no_texture);
    CHECK(store.stand_in_bindless(true) != no_texture);

    CHECK_FALSE(store.create({.width = 0, .height = 4}).valid());
    CHECK_FALSE(store.create({.width = 4, .height = 4, .rgba = {1, 2, 3}}).valid());

    const ImageData img{.width = 2, .height = 2, .rgba = std::vector<std::uint8_t>(16, 255), .srgb = true};
    const TextureHandle t = store.create(img);
    REQUIRE(t.valid());
    CHECK(store.get(t)->width == 2);
    CHECK(store.bindless_or(t, no_texture) != no_texture);

    const ImageData bigger{.width = 4, .height = 4, .rgba = std::vector<std::uint8_t>(64, 128), .srgb = true};
    CHECK(store.update(t, bigger));
    CHECK(store.get(t)->width == 4);
    CHECK_FALSE(store.update(TextureHandle{}, bigger));

    store.destroy(t);
    CHECK(store.get(t) == nullptr);
    CHECK(store.bindless_or(t, no_texture) == no_texture);
}

TEST_CASE("MaterialStore protects default material, resolves fallbacks, and packs GPU tables") {
    test::TestDevice device;
    if (!device) return;
    TextureStore textures(*device);
    ShaderStore shaders(*device, gpu::Format::RGBA8Unorm);
    MaterialStore materials(*device, textures, shaders);

    const MaterialHandle def = materials.default_material();
    REQUIRE(def.valid());
    materials.destroy(def);
    CHECK(materials.get(def) != nullptr);
    CHECK(materials.resolve_index(MaterialHandle{}) == def.index);

    const MaterialHandle m = materials.create({.base_color = {0.2f, 0.4f, 0.6f, 1.0f}, .roughness = 0.3f});
    REQUIRE(m.valid());
    CHECK(materials.resolve_index(m) == m.index);
    CHECK(materials.surface_shader_key(m) == 0u);
    CHECK_FALSE(materials.is_post(m));

    const ShaderParse post_parse = parse_ashader("shader_type post;\nvoid post() { COLOR = COLOR.bgr; }\n");
    REQUIRE(post_parse.errors.empty());
    const ScreenShaderBuild post_build = compile_screen_shader(post_parse.source, "unit_post");
    REQUIRE(post_build.ok());
    const ShaderHandle post_shader = shaders.create(post_build, "unit_post");
    REQUIRE(post_shader.valid());

    CHECK(materials.update(m, {.shader = post_shader}));
    CHECK(materials.is_post(m));
    CHECK(materials.surface_shader_key(m) == 0u); // post shaders draw no meshes

    GrowBuffer mat_buf, param_buf;
    materials.upload(mat_buf, param_buf);
    CHECK(mat_buf.buffer.valid());
    CHECK(param_buf.buffer.valid());
    mat_buf.destroy(*device);
    param_buf.destroy(*device);

    materials.destroy(m);
    CHECK(materials.get(m) == nullptr);
    CHECK(materials.resolve_index(m) == def.index);
    shaders.destroy(post_shader);
}

TEST_CASE("prepare_instances groups opaque draws, sorts blended back-to-front, and builds TLAS") {
    test::TestDevice device;
    if (!device) return;
    TextureStore textures(*device);
    ShaderStore shaders(*device, gpu::Format::RGBA8Unorm);
    MeshStore meshes(*device);
    MaterialStore materials(*device, textures, shaders);

    const bool rt = device->supports(gpu::Feature::RayTracing);
    const MeshHandle mesh_a = meshes.create(make_cube(), rt);
    const MeshHandle mesh_b = meshes.create(make_plane(2.0f), rt);
    const MeshHandle dead = meshes.create(make_cube(), rt);
    meshes.destroy(dead);

    const ShaderParse blend_parse = parse_ashader(
        "shader_type surface;\nrender_mode blend_mix;\nvoid surface() { ALBEDO = vec3(1.0); ALPHA = 0.5; }\n");
    REQUIRE(blend_parse.errors.empty());
    const SurfaceShaderBuild blend_build = compile_surface_shader(blend_parse.source, "unit_blend");
    REQUIRE(blend_build.ok());
    const ShaderHandle blend_shader = shaders.create(blend_build, "unit_blend");
    REQUIRE(blend_shader.valid());

    const MaterialHandle opaque_mat = materials.create({.base_color = {1, 0, 0, 1}});
    const MaterialHandle blend_mat = materials.create({.shader = blend_shader});

    const std::array items{
        MeshInstance{.mesh = mesh_b, .material = opaque_mat, .transform = translate({0, 0, 0})},
        MeshInstance{.mesh = dead, .material = opaque_mat, .transform = translate({0, 0, 0})},
        MeshInstance{.mesh = mesh_a, .material = blend_mat, .transform = translate({0, 0, 2})},  // nearer to camera at z=5
        MeshInstance{.mesh = mesh_a, .material = opaque_mat, .transform = translate({1, 0, 0})},
        MeshInstance{.mesh = mesh_a, .material = blend_mat, .transform = translate({0, 0, -2})}, // farther from camera at z=5
    };

    FrameSlot slot;
    FrameInstances prepared;
    prepare_instances(*device, {.meshes = items}, Vec3{0, 0, 5}, meshes, materials, slot, prepared, rt);

    // Opaque: mesh_a (index 0) sorted before mesh_b (index 1); dead mesh skipped.
    REQUIRE(prepared.order.size() == 2);
    CHECK(items[prepared.order[0]].mesh == mesh_a);
    CHECK(items[prepared.order[1]].mesh == mesh_b);
    // Blended: farthest (z=-2, items[4]) drawn before nearest (z=2, items[2]).
    REQUIRE(prepared.transparent_draws.size() == 2);
    CHECK(prepared.instances.size() == 4);
    CHECK(prepared.instances[prepared.transparent_draws[0].instance].model[3].z == doctest::Approx(-2.0f));
    CHECK(prepared.instances[prepared.transparent_draws[1].instance].model[3].z == doctest::Approx(2.0f));

    if (rt) {
        CHECK(prepared.tlas_instances.size() == 2); // opaque only
        auto cmd = device->begin_commands();
        CHECK(build_tlas(*device, cmd, prepared));
        CHECK(prepared.tlas.valid());
        CHECK(prepared.tlas_capacity >= 64);
        device->wait(device->submit(cmd));
    }

    prepared.destroy(*device);
    slot.destroy(*device);
    shaders.destroy(blend_shader);
}

TEST_CASE("ShaderStore handles custom RT surface rebuild error fallback and recovery") {
    test::TestDevice device;
    if (!device || !device->supports(gpu::Feature::RayTracing)) return;
    ShaderStore shaders(*device, gpu::Format::RGBA8Unorm);
    REQUIRE(shaders.ensure_rt_pipelines());

    const ShaderParse p = parse_ashader("shader_type surface;\nconst float k = 1.0;\nvoid surface() { ALBEDO = vec3(k); }\n");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "dup_store.ashader");
    REQUIRE(build.ok());

    const ShaderHandle a = shaders.create(build, "a");
    const ShaderHandle b = shaders.create(build, "b");
    REQUIRE(a.valid());
    REQUIRE(b.valid());
    CHECK(shaders.find_by_key(a.index + 1) != nullptr);
    CHECK(shaders.find_by_key(0) == nullptr);

    shaders.rebuild_rt_surfaces_if_dirty();
    CHECK_FALSE(shaders.error().empty());
    CHECK(shaders.rt_pipelines().rt_lighting.valid());
    CHECK(shaders.rt_pipelines().ddgi_trace.valid());

    shaders.destroy(b);
    shaders.rebuild_rt_surfaces_if_dirty();
    CHECK(shaders.error().empty());

    shaders.destroy(a);
    shaders.rebuild_rt_surfaces_if_dirty();
    CHECK(shaders.error().empty());
}
