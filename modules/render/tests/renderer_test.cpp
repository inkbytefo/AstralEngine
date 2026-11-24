#include "gpu_test_support.hpp"

#include <astral/render/renderer.hpp>
#include <astral/render/text.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <vector>

using namespace astral;

namespace {

constexpr std::uint32_t size = 64;

struct Rgb {
    int r, g, b;
};
Rgb rgb(std::uint32_t p) {
    return {static_cast<int>(p & 0xFF), static_cast<int>((p >> 8) & 0xFF), static_cast<int>((p >> 16) & 0xFF)};
}
int luminance(std::uint32_t p) {
    const Rgb c = rgb(p);
    return c.r + c.g + c.b;
}

/// Test shorthand: an instance with its material inline (converted by Fixture::inst()).
struct Item {
    MeshHandle mesh;
    Mat4 transform = Mat4::identity();
    Vec4 color{1, 1, 1, 1};
    float roughness = 0.6f;
    float metallic = 0.0f;
    float emissive = 0.0f;
};

struct Fixture {
    test::TestDevice device;
    std::deque<std::vector<MeshInstance>> converted; // keeps spans handed out by inst() alive
    std::unique_ptr<Renderer> renderer;
    gpu::TextureHandle target;
    gpu::BufferHandle readback;
    CameraData camera;

    /// The raster-path tests run with ray tracing off; ray-traced tests pass their own settings.
    explicit Fixture(RenderSettings settings = {.shadow_map_size = 512, .ray_tracing = false}) {
        if (!device) return;
        renderer = std::make_unique<Renderer>(*device, gpu::Format::RGBA8Unorm, settings);
        target = device->create_texture({.width = size, .height = size, .format = gpu::Format::RGBA8Unorm,
                                         .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
        readback = device->create_buffer({.size = size * size * 4, .usage = gpu::BufferUsage::TransferDst,
                                          .memory = gpu::MemoryUsage::GpuToCpu});
        look_from({0, 0, 5}, {0, 0, 0});
    }
    void look_from(Vec3 eye, Vec3 at) {
        camera.position = eye;
        camera.view = look_at(eye, at, std::abs(eye.y - at.y) > 4.9f && eye.x == at.x && eye.z == at.z ? Vec3{0, 0, -1} : Vec3{0, 1, 0});
        camera.projection = perspective(radians(60), 1.0f, 0.1f, 100.0f);
    }
    /// Materials are created for each item; the returned span lives as long as the fixture.
    std::span<const MeshInstance> inst(std::span<const Item> items) {
        auto& out = converted.emplace_back();
        for (const Item& i : items) {
            const MaterialHandle m = renderer->create_material(
                {.base_color = i.color, .roughness = i.roughness, .metallic = i.metallic,
                 .emissive = Vec3{i.color.x, i.color.y, i.color.z} * i.emissive});
            out.push_back({.mesh = i.mesh, .material = m, .transform = i.transform});
        }
        return out;
    }
    std::vector<std::uint32_t> draw(const RenderScene& scene) {
        auto cmd = device->begin_commands();
        renderer->render(cmd, target, camera, scene);
        cmd.transition(target, gpu::ResourceState::TransferSrc);
        cmd.copy_texture_to_buffer(target, readback);
        device->wait(device->submit(cmd));
        std::vector<std::uint32_t> pixels(size * size);
        device->read_buffer(readback, pixels.data(), pixels.size() * 4);
        return pixels;
    }
    /// Pixel where a world point lands on screen.
    std::uint32_t at(const std::vector<std::uint32_t>& pixels, Vec3 world) const {
        const Vec4 clip = camera.projection * camera.view * Vec4{world.x, world.y, world.z, 1};
        const float x = (clip.x / clip.w * 0.5f + 0.5f) * size;
        const float y = (clip.y / clip.w * 0.5f + 0.5f) * size;
        const auto px = static_cast<std::uint32_t>(std::clamp(x, 0.0f, size - 1.0f));
        const auto py = static_cast<std::uint32_t>(std::clamp(y, 0.0f, size - 1.0f));
        return pixels[py * size + px];
    }
    static std::uint32_t center(const std::vector<std::uint32_t>& p) { return p[(size / 2) * size + size / 2]; }
};

const DirectionalLight front_sun{.direction = {0, 0, -1}, .intensity = 2.0f, .shadows = false};

} // namespace

TEST_CASE("primitive meshes have consistent geometry") {
    const MeshData cube = make_cube();
    CHECK(cube.vertices.size() == 24);
    CHECK(cube.indices.size() == 36);
    CHECK(make_plane(10.0f).indices.size() == 6);
    const MeshData sphere = make_sphere(16, 8);
    for (auto i : sphere.indices) CHECK(i < sphere.vertices.size());
}

TEST_CASE("a lit mesh is drawn over the sky") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    const std::vector<Item> items{{.mesh = cube, .color = {1, 0, 0, 1}, .roughness = 1.0f}};
    auto pixels = f.draw({.meshes = f.inst(items), .sun = front_sun});
    const Rgb c = rgb(Fixture::center(pixels));
    CHECK(c.r > 150);
    CHECK(c.g < 60);
    const Rgb sky = rgb(pixels[0]); // top-left: sky, bluish
    CHECK(sky.b > sky.r);
    CHECK(f.renderer->stats().passes >= 3);
}

TEST_CASE("depth test keeps the nearer mesh in front") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    const std::vector<Item> items{{.mesh = cube, .transform = translate({0, 0, 1}), .color = {0, 1, 0, 1}, .roughness = 1.0f},
                                      {.mesh = cube, .transform = translate({0, 0, -1}), .color = {1, 0, 0, 1}, .roughness = 1.0f}};
    const Rgb c = rgb(Fixture::center(f.draw({.meshes = f.inst(items), .sun = front_sun})));
    CHECK(c.g > 150);
    CHECK(c.r < 60);
}

TEST_CASE("many instances of one mesh are one draw call") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    std::vector<Item> items;
    for (int i = 0; i < 1000; ++i) items.push_back({.mesh = cube, .transform = translate({0, 0, -static_cast<float>(i)})});
    f.draw({.meshes = f.inst(items), .sun = front_sun});
    CHECK(f.renderer->stats().draw_calls == 1);
    CHECK(f.renderer->stats().instances == 1000);
}

TEST_CASE("draw items with destroyed meshes are skipped") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    f.renderer->destroy(cube);
    const std::vector<Item> items{{.mesh = cube, .color = {1, 0, 0, 1}}};
    const Rgb c = rgb(Fixture::center(f.draw({.meshes = f.inst(items), .sun = front_sun})));
    CHECK(c.b > c.r); // sky, not a red cube
}

TEST_CASE("an SDF sphere is raymarched into the image") {
    Fixture f;
    if (!f.device) return;
    const std::vector<SdfShape> shapes{{.primitive = SdfPrimitive::Sphere, .size = {1, 0, 0, 0}, .color = {1, 0, 0, 1}, .roughness = 1.0f}};
    const Rgb c = rgb(Fixture::center(f.draw({.sdf = shapes, .sun = front_sun})));
    CHECK(c.r > 150);
    CHECK(c.g < 60);
    CHECK(f.renderer->stats().sdf_shapes == 1);
}

TEST_CASE("meshes and SDF shapes are depth-composited") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    SUBCASE("mesh in front") {
        const std::vector<Item> items{{.mesh = cube, .transform = translate({0, 0, 1.5f}), .color = {0, 1, 0, 1}}};
        const std::vector<SdfShape> shapes{{.size = {1, 0, 0, 0}, .color = {1, 0, 0, 1}}};
        const Rgb c = rgb(Fixture::center(f.draw({.meshes = f.inst(items), .sdf = shapes, .sun = front_sun})));
        CHECK(c.g > c.r);
    }
    SUBCASE("SDF in front") {
        const std::vector<Item> items{{.mesh = cube, .transform = translate({0, 0, -2}), .color = {0, 1, 0, 1}}};
        const std::vector<SdfShape> shapes{{.position = {0, 0, 1}, .size = {1, 0, 0, 0}, .color = {1, 0, 0, 1}}};
        const Rgb c = rgb(Fixture::center(f.draw({.meshes = f.inst(items), .sdf = shapes, .sun = front_sun})));
        CHECK(c.r > c.g);
    }
}

TEST_CASE("SDF subtraction carves a hole") {
    Fixture f;
    if (!f.device) return;
    // A big box with a sphere subtracted at the front-center: the center pixel sees deeper in,
    // so the box face at the center is gone (we look into the carved cavity).
    const std::vector<SdfShape> solid{{.primitive = SdfPrimitive::Box, .operation = SdfOperation::Union,
                                       .size = {1, 1, 1, 0}, .color = {1, 1, 1, 1}}};
    const std::vector<SdfShape> carved{solid[0], {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::Subtract,
                                                  .position = {0, 0, 1.2f}, .size = {0.6f, 0, 0, 0}}};
    const int solid_l = luminance(Fixture::center(f.draw({.sdf = solid, .sun = front_sun})));
    const int carved_l = luminance(Fixture::center(f.draw({.sdf = carved, .sun = front_sun})));
    CHECK(carved_l != solid_l);
}

TEST_CASE("point lights light the scene") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    const std::vector<Item> items{{.mesh = cube, .color = {1, 1, 1, 1}}};
    const DirectionalLight dark{.intensity = 0.0f, .shadows = false};
    const Environment no_ambient{.ambient = 0.0f};
    const Rgb unlit = rgb(Fixture::center(f.draw({.meshes = f.inst(items), .sun = dark, .environment = no_ambient})));
    const std::vector<PointLight> lights{{.position = {0, 0, 2}, .color = {1, 0, 0}, .intensity = 20, .radius = 10}};
    const Rgb lit = rgb(Fixture::center(f.draw({.meshes = f.inst(items), .lights = lights, .sun = dark, .environment = no_ambient})));
    CHECK(unlit.r < 20);
    CHECK(lit.r > 100);
    CHECK(lit.g < 30);
}

TEST_CASE("the sun casts shadows from meshes") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    MeshHandle plane = f.renderer->create_mesh(make_plane(1.0f));
    f.look_from({0, 8, 6}, {0, 0, 0});
    const std::vector<Item> items{{.mesh = plane, .transform = scale({20, 1, 20})},
                                      {.mesh = cube, .transform = translate({0, 1, 0})}};
    const DirectionalLight sun{.direction = {1, -1, 0}, .intensity = 3.0f, .shadows = true};
    auto pixels = f.draw({.meshes = f.inst(items), .sun = sun});
    const int shadowed = luminance(f.at(pixels, {1.3f, 0, 0}));  // behind the cube, away from the sun
    const int lit = luminance(f.at(pixels, {-2.5f, 0, 0}));      // open ground
    CHECK(shadowed < lit - 60);
    CHECK(f.renderer->stats().shadow_draw_calls == 2);
}

TEST_CASE("a camera inside an SDF shape sees through it instead of producing NaNs") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    const std::vector<Item> items{{.mesh = cube, .transform = translate({0, 0, -3}), .color = {1, 0, 0, 1}, .roughness = 1.0f}};
    const std::vector<SdfShape> shapes{{.position = {0, 0, 5}, .size = {1.5f, 0, 0, 0}, .color = {0, 1, 0, 1}}}; // camera at z=5
    const Rgb c = rgb(Fixture::center(f.draw({.meshes = f.inst(items), .sdf = shapes, .sun = front_sun})));
    CHECK(c.r > 100); // the red cube behind, not a black/NaN frame
}

TEST_CASE("large user meshes still cast and receive shadows") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    MeshHandle ground = f.renderer->create_mesh(make_plane(20.0f)); // big mesh at identity
    f.look_from({0, 8, 6}, {0, 0, 0});
    const std::vector<Item> items{{.mesh = ground}, {.mesh = cube, .transform = translate({0, 1, 0})}};
    auto pixels = f.draw({.meshes = f.inst(items), .sun = {.direction = {1, -1, 0}, .intensity = 3.0f}});
    CHECK(luminance(f.at(pixels, {1.3f, 0, 0})) < luminance(f.at(pixels, {-2.5f, 0, 0})) - 60);
}

TEST_CASE("shadows stay attached to their casters in large scenes") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    MeshHandle plane = f.renderer->create_mesh(make_plane(1.0f));
    f.look_from({0, 8, 6}, {0, 0, 0});
    const std::vector<Item> items{{.mesh = plane, .transform = scale({80, 1, 80})},
                                      {.mesh = cube, .transform = translate({0, 0.5f, 0})}}; // resting on the ground
    auto pixels = f.draw({.meshes = f.inst(items), .sun = {.direction = {1, -1, 0}, .intensity = 3.0f}});
    // Right next to the cube's base on the far side from the sun: must be in shadow.
    CHECK(luminance(f.at(pixels, {0.75f, 0, 0})) < luminance(f.at(pixels, {-2.5f, 0, 0})) - 60);
}

// --- Ray-traced path ----------------------------------------------------------------------

namespace {

/// Sky light at a realistic ratio to the sun (the default sky is a bright overcast sky).
const Environment dim_sky{.sky = {0.08f, 0.1f, 0.13f}, .ground = {0.03f, 0.03f, 0.03f}};

RenderSettings rt_settings() {
    RenderSettings s;
    s.gi = {.probes_x = 4, .probes_y = 4, .probes_z = 4, .rays_per_probe = 32};
    return s;
}

/// Renders several frames so temporal accumulation (denoiser, DDGI, TAA) converges.
std::vector<std::uint32_t> converge(Fixture& f, const RenderScene& scene, int frames = 12) {
    std::vector<std::uint32_t> pixels;
    for (int i = 0; i < frames; ++i) pixels = f.draw(scene);
    return pixels;
}

bool rt_available(Fixture& f) {
    if (!f.device) return false;
    if (!f.renderer->ray_tracing()) {
        MESSAGE("no ray tracing — skipping");
        return false;
    }
    return true;
}

} // namespace

TEST_CASE("ray-traced path renders a lit mesh with the full pass chain") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const Item item{.mesh = cube, .color = {1, 0, 0, 1}, .roughness = 1.0f}; // no highlight toward the camera
    const auto pixels = converge(f, {.meshes = f.inst(std::span(&item, 1)), .sun = front_sun, .environment = dim_sky}, 3);
    const Rgb c = rgb(Fixture::center(pixels));
    CHECK(c.r > 120);
    CHECK(c.r > c.g + 80); // a red cube (plus a faint, physically correct white specular)
    CHECK(f.renderer->stats().ray_traced);
    CHECK(f.renderer->stats().gi_probes == 64);
    CHECK(f.renderer->stats().passes >= 10);
}

TEST_CASE("ray-traced sun shadows darken what lies under a caster") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    f.look_from({0, 6, 0.01f}, {0, 0, 0});
    const MeshHandle plane = f.renderer->create_mesh(make_plane(10.0f));
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const std::array items{Item{.mesh = plane, .color = {0.8f, 0.8f, 0.8f, 1}},
                           Item{.mesh = cube, .transform = translate({0, 1.5f, 0}), .color = {0.8f, 0.8f, 0.8f, 1}}};
    const RenderScene scene{.meshes = f.inst(items), .sun = {.direction = {0.6f, -1, 0}, .intensity = 3.0f}, .environment = dim_sky};
    const auto pixels = converge(f, scene);
    // The cube sits 1.5 above the floor; the sun moves its shadow toward +x by ~0.6 * 1.5.
    CHECK(luminance(f.at(pixels, {1.0f, 0, 0})) < luminance(f.at(pixels, {-2.5f, 0, 0})) - 60);
}

TEST_CASE("a rect area light lights the floor and is visible to the camera") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    f.look_from({0, 3, 6}, {0, 0.5f, 0});
    const MeshHandle plane = f.renderer->create_mesh(make_plane(10.0f));
    const Item floor{.mesh = plane, .color = {0.8f, 0.8f, 0.8f, 1}};
    // cross(right, up) = -y: the panel shines down; two-sided so the camera above sees it glow too.
    const RectLight panel{.position = {0, 1.5f, 0}, .right = {0.5f, 0, 0}, .up = {0, 0, 0.5f}, .intensity = 8.0f,
                          .two_sided = true};
    RenderScene scene{.meshes = f.inst(std::span(&floor, 1)), .sun = {.intensity = 0}, .environment = {.sky = {}, .ground = {}}};
    const auto dark = converge(f, scene, 2);
    scene.rect_lights = std::span(&panel, 1);
    f.renderer->reset_history();
    const auto lit = converge(f, scene);
    CHECK(luminance(f.at(lit, {0, 0, 0})) > luminance(f.at(dark, {0, 0, 0})) + 100);
    CHECK(luminance(f.at(lit, {0, 0, 0})) > luminance(f.at(lit, {4, 0, 0})) + 40); // falls off with distance
    CHECK(rgb(f.at(lit, {0, 1.5f, 0})).r > 240);                                 // the panel itself glows
    CHECK(f.renderer->stats().rect_lights == 1);
}

TEST_CASE("a mirror reflects what is behind the camera") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    // A mirror wall at z = -1 facing the camera, and a red block behind the camera (not visible directly).
    const std::array items{
        Item{.mesh = cube, .transform = translate({0, 0, -1.5f}) * scale({6, 6, 1}), .color = {0.95f, 0.95f, 0.95f, 1},
                 .roughness = 0.02f, .metallic = 1.0f},
        Item{.mesh = cube, .transform = translate({0, 0, 9}) * scale({6, 6, 1}), .color = {1, 0, 0, 1}},
    };
    const DirectionalLight sun{.direction = {0, 0, 1}, .intensity = 3.0f}; // lights the red block's mirror-facing side
    const auto pixels = converge(f, {.meshes = f.inst(items), .sun = sun}, 4);
    const Rgb c = rgb(Fixture::center(pixels));
    CHECK(c.r > 100);
    CHECK(c.r > c.g + 60);
}

TEST_CASE("global illumination bleeds a wall's colour onto a white floor") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    f.look_from({0, 2, 5}, {0, 0, 0});
    const MeshHandle plane = f.renderer->create_mesh(make_plane(6.0f));
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    std::array items{Item{.mesh = plane, .color = {0.9f, 0.9f, 0.9f, 1}},
                     Item{.mesh = cube, .transform = translate({-1.6f, 1, 0}) * scale({0.2f, 2, 4}), .color = {0.9f, 0.05f, 0.05f, 1}}};
    // Sky light only: next to the wall, part of the sky is replaced by light bounced off the wall.
    // Exposure keeps the floor in the mid-tones, where the tone curve does not hide differences.
    const RenderScene scene{.meshes = f.inst(items), .sun = {.intensity = 0.0f},
                            .environment = {.sky = {1, 1, 1}, .ground = {0.3f, 0.3f, 0.3f}, .exposure = 0.5f}};
    const auto with_wall = converge(f, scene, 16);
    const Rgb red_bounce = rgb(f.at(with_wall, {-1.2f, 0, 0.5f}));
    items[1].color = {0.9f, 0.9f, 0.9f, 1};
    f.renderer->reset_history();
    RenderScene white_scene = scene;
    white_scene.meshes = f.inst(items);
    const auto white_wall = converge(f, white_scene, 16);
    const Rgb neutral = rgb(f.at(white_wall, {-1.2f, 0, 0.5f}));
    CHECK(std::abs(neutral.r - neutral.g) < 4);
    CHECK(red_bounce.r - red_bounce.g > neutral.r - neutral.g + 8); // red light bounced off the wall
    CHECK(red_bounce.g < neutral.g);                                // ...which reflects little green
}

TEST_CASE("SDF shapes take part in ray-traced lighting") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    f.look_from({0, 6, 0.01f}, {0, 0, 0});
    const MeshHandle plane = f.renderer->create_mesh(make_plane(10.0f));
    const Item floor{.mesh = plane, .color = {0.8f, 0.8f, 0.8f, 1}};
    // A grey ball (a white one bounces so much light back into its own shadow that it washes out).
    const SdfShape ball{.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::Union, .position = {0, 1.5f, 0},
                        .size = {0.8f, 0, 0, 0}, .color = {0.3f, 0.3f, 0.3f, 1}};
    const auto pixels = converge(f, {.meshes = f.inst(std::span(&floor, 1)), .sdf = std::span(&ball, 1),
                                     .sun = {.direction = {0.6f, -1, 0}, .intensity = 3.0f}, .environment = dim_sky});
    // The shadow is centred at x = 0.9; seen from above, x < ~1.07 is hidden behind the ball itself.
    CHECK(luminance(f.at(pixels, {1.4f, 0, 0})) < luminance(f.at(pixels, {-2.5f, 0, 0})) - 60);
}







// --- API: materials, runtime settings, motion -------------------------------------------------

TEST_CASE("materials are shared handles; updating one recolours every instance using it") {
    Fixture f;
    if (!f.device) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle paint = f.renderer->create_material({.base_color = {1, 0, 0, 1}, .roughness = 1.0f});
    const MeshInstance inst{.mesh = cube, .material = paint};
    const RenderScene scene{.meshes = std::span(&inst, 1), .sun = front_sun};
    CHECK(rgb(Fixture::center(f.draw(scene))).r > 150);
    REQUIRE(f.renderer->update_material(paint, {.base_color = {0, 0, 1, 1}, .roughness = 1.0f}));
    const Rgb blue = rgb(Fixture::center(f.draw(scene)));
    CHECK(blue.b > 150);
    CHECK(blue.r < 60);
    // A destroyed material falls back to the default (white) material, never garbage.
    f.renderer->destroy(paint);
    CHECK_FALSE(f.renderer->update_material(paint, {}));
    const Rgb white = rgb(Fixture::center(f.draw(scene)));
    CHECK(std::abs(white.r - white.b) < 20);
    CHECK(white.r > 100);
    // The default material cannot be destroyed.
    f.renderer->destroy(f.renderer->default_material());
    CHECK(f.renderer->material(f.renderer->default_material()) != nullptr);
}

TEST_CASE("settings change at runtime: ray tracing on and off") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const Item item{.mesh = cube, .color = {1, 0, 0, 1}, .roughness = 1.0f};
    const RenderScene scene{.meshes = f.inst(std::span(&item, 1)), .sun = front_sun, .environment = dim_sky};
    f.draw(scene);
    CHECK(f.renderer->stats().ray_traced);
    RenderSettings raster = f.renderer->settings();
    raster.ray_tracing = false;
    f.renderer->set_settings(raster);
    CHECK_FALSE(f.renderer->ray_tracing());
    CHECK(rgb(Fixture::center(f.draw(scene))).r > 120);
    CHECK_FALSE(f.renderer->stats().ray_traced);
    // Meshes created while ray tracing was off get their acceleration structures on the way back.
    const MeshHandle later = f.renderer->create_mesh(make_cube());
    raster.ray_tracing = true;
    f.renderer->set_settings(raster);
    REQUIRE(f.renderer->ray_tracing());
    const Item item2{.mesh = later, .color = {1, 0, 0, 1}, .roughness = 1.0f};
    const auto pixels = converge(f, {.meshes = f.inst(std::span(&item2, 1)), .sun = front_sun, .environment = dim_sky}, 2);
    CHECK(rgb(Fixture::center(pixels)).r > 120);
    CHECK(f.renderer->stats().ray_traced);
}

TEST_CASE("moving meshes get per-object motion vectors") {
    RenderSettings s = rt_settings();
    s.debug_view = DebugView::Motion;
    s.bloom_strength = 0;
    s.taa_blend = 1.0f; // show this frame's motion only
    Fixture f(s);
    if (!rt_available(f)) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle mat = f.renderer->create_material({});
    MeshInstance moving{.mesh = cube, .material = mat, .transform = translate({-1, 0, 0}),
                        .previous_transform = translate({-1.3f, 0, 0})};
    const MeshInstance still{.mesh = cube, .material = mat, .transform = translate({1, 0, 0})};
    const std::array scene_items{moving, still};
    const RenderScene scene{.meshes = scene_items, .sun = front_sun, .environment = dim_sky};
    f.draw(scene);
    const auto pixels = f.draw(scene); // second frame: the camera has a history, so only objects move
    CHECK(rgb(f.at(pixels, {-1, 0, 0.5f})).r > 40); // moved right on screen
    CHECK(rgb(f.at(pixels, {1, 0, 0.5f})).r < 10);  // did not move
}

// --- Textures --------------------------------------------------------------------------------

namespace {
ImageData solid_halves(std::array<std::uint8_t, 4> left, std::array<std::uint8_t, 4> right, bool srgb = true) {
    ImageData img{.width = 16, .height = 16, .rgba = {}, .srgb = srgb, .format = ImageFormat::RGBA8, .levels = {}};
    for (std::uint32_t y = 0; y < 16; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            const auto& c = x < 8 ? left : right;
            img.rgba.insert(img.rgba.end(), c.begin(), c.end());
        }
    }
    return img;
}
} // namespace

TEST_CASE("base colour textures are sampled with the mesh uvs, and can be replaced in place") {
    Fixture f;
    if (!f.device) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const TextureHandle tex = f.renderer->create_texture(solid_halves({255, 0, 0, 255}, {0, 0, 255, 255}));
    REQUIRE(tex.valid());
    const MaterialHandle mat = f.renderer->create_material({.roughness = 1.0f, .base_color_texture = tex});
    const MeshInstance inst{.mesh = cube, .material = mat, .transform = scale({2, 2, 2})};
    const RenderScene scene{.meshes = std::span(&inst, 1), .sun = front_sun};
    auto pixels = f.draw(scene);
    const Rgb left = rgb(f.at(pixels, {-0.5f, 0, 1})), right = rgb(f.at(pixels, {0.5f, 0, 1}));
    CHECK(left.r > left.b + 80); // red half (the sky adds a little blue ambient everywhere)
    CHECK(right.b > right.r + 80);
    REQUIRE(f.renderer->update_texture(tex, solid_halves({0, 255, 0, 255}, {0, 255, 0, 255})));
    pixels = f.draw(scene);
    CHECK(rgb(f.at(pixels, {-0.5f, 0, 1})).g > 120);
    CHECK(rgb(f.at(pixels, {0.5f, 0, 1})).g > 120);
    // Bad images are refused; destroying a texture used by a material just drops the texture.
    CHECK_FALSE(f.renderer->create_texture({.width = 4, .height = 4, .rgba = {1, 2, 3}, .srgb = true, .format = ImageFormat::RGBA8, .levels = {}}).valid());
    f.renderer->destroy(tex);
    CHECK(rgb(Fixture::center(f.draw(scene))).g > 120); // material colour (white) under the sun: bright
}

TEST_CASE("normal maps tilt the shading normal") {
    Fixture f;
    if (!f.device) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    // Flat map (0.5, 0.5, 1) vs a map tilted strongly along +u: lit from the front, the tilt darkens.
    const TextureHandle flat = f.renderer->create_texture(solid_halves({128, 128, 255, 255}, {128, 128, 255, 255}, false));
    const TextureHandle tilted = f.renderer->create_texture(solid_halves({250, 128, 60, 255}, {250, 128, 60, 255}, false));
    const MaterialHandle a = f.renderer->create_material({.roughness = 1.0f, .normal_texture = flat});
    const MaterialHandle b = f.renderer->create_material({.roughness = 1.0f, .normal_texture = tilted});
    const DirectionalLight sun{.direction = {0, 0, -1}, .intensity = 1.0f, .shadows = false};
    const Environment dark{.sky = {}, .ground = {}, .ambient = 0};
    const MeshInstance ia{.mesh = cube, .material = a};
    const MeshInstance ib{.mesh = cube, .material = b};
    const int lit_flat = luminance(Fixture::center(f.draw({.meshes = std::span(&ia, 1), .sun = sun, .environment = dark})));
    const int lit_tilted = luminance(Fixture::center(f.draw({.meshes = std::span(&ib, 1), .sun = sun, .environment = dark})));
    CHECK(lit_tilted < lit_flat - 60);
}

TEST_CASE("update_mesh swaps geometry behind the same handle") {
    Fixture f;
    if (!f.device) return;
    const MeshHandle mesh = f.renderer->create_mesh(make_cube());
    const MeshInstance inst{.mesh = mesh, .material = f.renderer->create_material({.base_color = {1, 0, 0, 1}, .roughness = 1.0f}),
                            .transform = translate({1.5f, 0, 0})};
    const RenderScene scene{.meshes = std::span(&inst, 1), .sun = front_sun};
    CHECK(rgb(Fixture::center(f.draw(scene))).b > rgb(Fixture::center(f.draw(scene))).r); // centre: sky
    MeshData wide = make_cube();
    for (auto& v : wide.vertices) v.position.x *= 6.0f; // now reaches the screen centre
    REQUIRE(f.renderer->update_mesh(mesh, wide));
    CHECK(rgb(Fixture::center(f.draw(scene))).r > 120);
    CHECK_FALSE(f.renderer->update_mesh(MeshHandle{}, wide));
}

namespace {
/// A BC5 image whose every texel is (x, y): BC4 blocks with both endpoints equal and index 0.
ImageData constant_bc5(std::uint8_t x, std::uint8_t y) {
    ImageData img{.width = 16, .height = 16, .rgba = {}, .srgb = false, .format = ImageFormat::BC5, .levels = {}};
    for (std::uint32_t side = 16;; side /= 2) {
        const std::uint32_t blocks = ((side + 3) / 4) * ((side + 3) / 4);
        std::vector<std::uint8_t> level;
        for (std::uint32_t b = 0; b < blocks; ++b) {
            for (std::uint8_t v : {x, y}) {
                const std::uint8_t block[8] = {v, v, 0, 0, 0, 0, 0, 0};
                level.insert(level.end(), block, block + 8);
            }
        }
        img.levels.push_back(std::move(level));
        if (side == 1) break;
    }
    return img;
}
} // namespace

TEST_CASE("ready-made compressed mip chains: two-channel normal maps get their Z rebuilt") {
    Fixture f;
    if (!f.device || !f.renderer->supports(ImageFormat::BC5)) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const TextureHandle flat = f.renderer->create_texture(constant_bc5(128, 128));
    const TextureHandle tilted = f.renderer->create_texture(constant_bc5(250, 128));
    REQUIRE(flat.valid());
    REQUIRE(tilted.valid());
    const MaterialHandle a = f.renderer->create_material({.roughness = 1.0f, .normal_texture = flat});
    const MaterialHandle b = f.renderer->create_material({.roughness = 1.0f, .normal_texture = tilted});
    const DirectionalLight sun{.direction = {0, 0, -1}, .intensity = 1.0f, .shadows = false};
    const Environment dark{.sky = {}, .ground = {}, .ambient = 0};
    const MeshInstance ia{.mesh = cube, .material = a};
    const MeshInstance ib{.mesh = cube, .material = b};
    const int lit_flat = luminance(Fixture::center(f.draw({.meshes = std::span(&ia, 1), .sun = sun, .environment = dark})));
    const int lit_tilted = luminance(Fixture::center(f.draw({.meshes = std::span(&ib, 1), .sun = sun, .environment = dark})));
    CHECK(lit_flat > 150);                 // (0, 0) -> Z = 1: facing the light
    CHECK(lit_tilted < lit_flat - 60);

    // A level of the wrong size is refused.
    ImageData broken = constant_bc5(128, 128);
    broken.levels[1].pop_back();
    CHECK_FALSE(f.renderer->create_texture(broken).valid());
}

TEST_CASE("the renderer reports GPU time per pass a few frames later") {
    Fixture f;
    if (!f.device || !f.device->supports(gpu::Feature::Timestamps)) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MeshInstance inst{.mesh = cube, .material = {}};
    for (int i = 0; i < 6; ++i) f.draw({.meshes = std::span(&inst, 1), .sun = front_sun});
    const auto timings = f.renderer->gpu_timings();
    REQUIRE_FALSE(timings.empty());
    float sum = 0;
    for (const PassTiming& t : timings) {
        CHECK_FALSE(t.name.empty());
        CHECK(t.ms >= 0.0f);
        sum += t.ms;
    }
    CHECK(f.renderer->gpu_frame_ms() > 0.0f);
    CHECK(f.renderer->gpu_frame_ms() == doctest::Approx(sum).epsilon(0.05));
}

TEST_CASE("debug lines: depth-tested ones hide behind meshes, on-top ones do not (raster and ray-traced)") {
    for (const bool rt : {false, true}) {
        CAPTURE(rt);
        Fixture f(RenderSettings{.shadow_map_size = 512, .ray_tracing = rt});
        if (!f.device) return;
        if (rt && !f.renderer->ray_tracing()) continue;
        const MeshHandle cube = f.renderer->create_mesh(make_cube());
        const MeshInstance wall{.mesh = cube, .material = {}, .transform = translate({0, 0, 1}) * scale({1.0f, 1.0f, 0.2f})};
        // A vertical line through the centre, behind the wall (z = -1), and one through x = 0.2 on top.
        DebugDraw debug;
        debug.line({0, -2, -1}, {0, 2, -1}, {1, 0, 0, 1});
        debug.line({0.2f, -2, -1}, {0.2f, 2, -1}, {0, 1, 0, 1}, {.on_top = true});
        debug.line({0, 1.6f, -1}, {0, 1.9f, -1}, {0, 0, 1, 1}); // tested, but nothing in front of it
        const RenderScene scene{.meshes = std::span(&wall, 1), .sun = front_sun, .debug_lines = debug.lines()};
        const auto pixels = f.draw(scene);
        const Rgb hidden = rgb(f.at(pixels, {0, 0, -1}));
        const Rgb on_top = rgb(f.at(pixels, {0.2f, 0, -1}));
        const Rgb free = rgb(f.at(pixels, {0, 1.75f, -1}));
        CHECK_FALSE((hidden.r > 200 && hidden.g < 60)); // the wall covers the red line
        CHECK(on_top.g > 200);
        CHECK(on_top.r < 80);
        CHECK(free.b > 200);
    }
}

TEST_CASE("DebugDraw shapes and lifetimes") {
    DebugDraw d;
    d.box({0, 0, 0}, {1, 1, 1}, Quat{}, {1, 1, 1, 1});
    CHECK(d.lines().size() == 12);
    d.sphere({0, 0, 0}, 1, {1, 1, 1, 1}, {.seconds = 1.0f}, 16);
    CHECK(d.lines().size() == 12 + 48);
    for (const DebugLine& l : d.lines().subspan(12)) CHECK(length(l.a) == doctest::Approx(1.0f).epsilon(1e-4));
    d.advance(0.016f); // the box was for one frame; the sphere stays
    CHECK(d.lines().size() == 48);
    d.advance(2.0f);
    CHECK(d.lines().empty());
    d.arrow({0, 0, 0}, {0, 0, 0}, {1, 1, 1, 1}); // degenerate: just the shaft
    CHECK(d.lines().size() == 1);
    d.clear();
    d.capsule({0, 0, 0}, Quat{}, 0.5f, 1.0f, {1, 1, 1, 1});
    CHECK(d.lines().size() > 40);
}

namespace {
/// Two triangles covering [x0, x1) x [y0, y1) pixels.
void quad(std::vector<UiVertex>& v, std::vector<std::uint32_t>& idx, float x0, float y0, float x1, float y1, std::uint32_t color) {
    const auto base = static_cast<std::uint32_t>(v.size());
    v.push_back({{x0, y0}, {0, 0}, color});
    v.push_back({{x1, y0}, {1, 0}, color});
    v.push_back({{x1, y1}, {1, 1}, color});
    v.push_back({{x0, y1}, {0, 1}, color});
    for (std::uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) idx.push_back(base + i);
}
} // namespace

TEST_CASE("the 2D overlay draws over the image with colours, textures and clip rectangles") {
    Fixture f;
    if (!f.device) return;
    const float s = static_cast<float>(size);
    std::vector<UiVertex> v;
    std::vector<std::uint32_t> idx;
    quad(v, idx, 0, 0, s / 2, s / 2, 0xFF0000FFu);       // red, top-left quarter
    quad(v, idx, s / 2, 0, s, s / 2, 0xFFFFFFFFu);       // textured (green), top-right
    quad(v, idx, 0, s / 2, s, s, 0x80FF0000u);           // half-transparent blue, bottom half, clipped to the left half
    const TextureHandle green = f.renderer->create_texture(solid_halves({0, 255, 0, 255}, {0, 255, 0, 255}, false));
    const UiCommand cmds[] = {
        {.first_index = 0, .index_count = 6},
        {.first_index = 6, .index_count = 6, .texture = green},
        {.first_index = 12, .index_count = 6, .clip = {0, 0, s / 2, s}},
    };
    const UiLayer layer{.vertices = v, .indices = idx, .commands = cmds};
    const auto pixels = f.draw({.ui = std::span(&layer, 1)});
    auto px = [&](float x, float y) { return rgb(pixels[static_cast<std::size_t>(y) * size + static_cast<std::size_t>(x)]); };
    const Rgb red = px(s * 0.25f, s * 0.25f), tex = px(s * 0.75f, s * 0.25f);
    const Rgb blue = px(s * 0.25f, s * 0.75f), clipped = px(s * 0.75f, s * 0.75f);
    CHECK(red.r > 240);
    CHECK(red.g < 20);
    CHECK(tex.g > 240);
    CHECK(tex.r < 20);
    CHECK(blue.b > 100); // blended over the sky
    CHECK(blue.b > clipped.b + 50); // the right half was clipped away
}

TEST_CASE("fonts: distance-field glyphs with Turkish letters, measuring, lines and alignment") {
    std::string error;
    auto font = Font::create(default_font_ttf(), {}, &error);
    REQUIRE_MESSAGE(font, error);
    for (char32_t c : {U'A', U'ğ', U'Ğ', U'ş', U'Ş', U'ı', U'İ', U'ö', U'ç', U'ü', U'₺', U'€'}) {
        CAPTURE(static_cast<std::uint32_t>(c));
        REQUIRE(font->glyph(c));
        CHECK(font->glyph(c)->advance > 0.0f);
    }
    CHECK(font->glyph(U' ')->size.x == 0.0f); // a space has no quad, only an advance
    CHECK_FALSE(font->glyph(U'一'));    // CJK is not in the default ranges
    CHECK_FALSE(Font::create(std::vector<std::uint8_t>{1, 2, 3}, {}, &error));

    const Vec2 one = font->measure("Skor", 20);
    const Vec2 two = font->measure("Skor", 40);
    CHECK(one.x > 20.0f);
    CHECK(two.x == doctest::Approx(one.x * 2).epsilon(0.02));
    CHECK(font->measure("a\nb", 20).y == doctest::Approx(font->line_height(20) * 2));
    CHECK(font->measure("AV", 40).x < font->measure("A", 40).x + font->measure("V", 40).x); // kerning

    UiCanvas canvas;
    const Vec2 text_size = canvas.text(*font, "Merhaba dünya", {100, 10}, {.size = 30, .align = TextAlign::Center});
    float min_x = 1e9f, max_x = -1e9f;
    for (const UiVertex& v : canvas.layer().vertices) {
        min_x = std::min(min_x, v.position.x);
        max_x = std::max(max_x, v.position.x);
    }
    CHECK(min_x < 100 - text_size.x * 0.4f); // centred around x = 100
    CHECK(max_x > 100 + text_size.x * 0.4f);
    CHECK(canvas.layer().commands.size() == 1); // one draw for all glyphs
    canvas.rect({0, 0}, {10, 10}, {1, 0, 0, 1});
    CHECK(canvas.layer().commands.size() == 2);
    std::size_t at = 0;
    CHECK(next_codepoint("ğ", at) == U'ğ');
    at = 0;
    CHECK(next_codepoint(std::string_view("\xC4", 1), at) == 0xFFFD); // cut sequence
}

TEST_CASE("HUD text renders crisp on the GPU") {
    Fixture f;
    if (!f.device) return;
    auto font = Font::create(default_font_ttf());
    REQUIRE(font);
    font->upload(*f.renderer);
    UiCanvas canvas;
    canvas.rect({0, 0}, {64, 64}, {0, 0, 0, 1}); // black backdrop
    canvas.text(*font, "I", {32, 8}, {.size = 48, .color = {1, 1, 1, 1}, .align = TextAlign::Center});
    const UiLayer layer = canvas.layer();
    const auto pixels = f.draw({.ui = std::span(&layer, 1)});
    // The stem of the "I" runs down the middle; a little to the side is background.
    const Rgb stem = rgb(pixels[32 * size + 32]);
    const Rgb beside = rgb(pixels[32 * size + 20]);
    CHECK(stem.r > 200);
    CHECK(beside.r < 30);
}

TEST_CASE("a .ashader material draws with its uniforms; alpha test cuts holes; vertex() moves vertices") {
    Fixture f;
    if (!f.device) return;
    const ShaderParse p = parse_ashader(R"(shader_type surface;
render_mode alpha_test;
uniform vec4 tint : source_color = vec4(1.0);
uniform float lift = 0.0;
void vertex() { VERTEX.y += lift; }
void surface() {
    ALBEDO = tint.rgb;
    ROUGHNESS = 1.0;
    ALPHA = UV.x < 0.5 ? 0.0 : 1.0;   // left half of every face is cut away
}
)");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "test.ashader");
    REQUIRE(build.ok());
    const ShaderHandle shader = f.renderer->create_shader(build, "test");
    REQUIRE(shader.valid());
    MeshHandle quad = f.renderer->create_mesh(make_plane(1.0f));
    // The plane faces +y: look down at it. tint = green, lift = 0.
    f.look_from({0, 5, 0}, {0, 0, 0});
    const MaterialHandle mat = f.renderer->create_material(
        {.shader = shader, .params = {0, 1, 0, 1, 0, 0, 0, 0}, .textures = {}});
    const MeshInstance items[] = {{.mesh = quad, .material = mat, .transform = scale(Vec3{2, 1, 2})}};
    const DirectionalLight top{.direction = {0, -1, 0}, .intensity = 2.0f, .shadows = false};
    const auto empty = f.draw({.sun = top});
    auto pixels = f.draw({.meshes = items, .sun = top});
    // UV.x grows along +x: the +x half is green, the -x half shows the background.
    const Rgb kept = rgb(f.at(pixels, {0.5f, 0, 0}));
    const Rgb cut = rgb(f.at(pixels, {-0.5f, 0, 0}));
    const Rgb background = rgb(f.at(empty, {-0.5f, 0, 0}));
    CHECK(kept.g > 100);
    CHECK(kept.r < 60);
    CHECK(std::abs(cut.g - background.g) < 8);
    CHECK(std::abs(cut.r - background.r) < 8);
    // Raise the plane with the uniform: still green where the raised plane is seen.
    f.renderer->update_material(mat, {.shader = shader, .params = {1, 0, 0, 1, 2.0f, 0, 0, 0}, .textures = {}});
    pixels = f.draw({.meshes = items, .sun = top});
    const Rgb red = rgb(f.at(pixels, {0.5f, 2.0f, 0}));
    CHECK(red.r > 100);
    CHECK(red.g < 60);
    f.renderer->destroy(shader);
}

TEST_CASE("create_shader refuses a failed build") {
    Fixture f;
    if (!f.device) return;
    CHECK_FALSE(f.renderer->create_shader(SurfaceShaderBuild{}).valid());
}

TEST_CASE("ray-traced reflections show a .ashader material the way the raster path does") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    const ShaderParse p = parse_ashader(R"(shader_type surface;
uniform vec3 glow = vec3(0.0, 1.0, 0.0);
uniform sampler2D mask : source_color;
void surface() {
    ALBEDO = vec3(0.0);
    EMISSION = glow * 3.0 * texture(mask, UV).r;   // an unset texture reads as white
}
)");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "glow.ashader");
    REQUIRE(build.ok());
    const ShaderHandle shader = f.renderer->create_shader(build, "glow");
    REQUIRE(shader.valid());
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    // A mirror wall in front of the camera; behind the camera a block whose surface is the custom shader.
    const MaterialHandle mirror = f.renderer->create_material(
        {.base_color = {0.95f, 0.95f, 0.95f, 1}, .roughness = 0.02f, .metallic = 1.0f});
    const MaterialHandle glow = f.renderer->create_material({.shader = shader, .params = {0, 1, 0, 0, 0, 0, 0, 0}});
    const std::array items{
        MeshInstance{.mesh = cube, .material = mirror, .transform = translate({0, 0, -1.5f}) * scale({6, 6, 1})},
        MeshInstance{.mesh = cube, .material = glow, .transform = translate({0, 0, 9}) * scale({6, 6, 1})},
    };
    const DirectionalLight sun{.direction = {0, 0, 1}, .intensity = 0.0f};
    const auto pixels = converge(f, {.meshes = items, .sun = sun}, 4);
    const Rgb c = rgb(Fixture::center(pixels));
    CHECK(c.g > 100);
    CHECK(c.g > c.r + 60);
    CHECK(c.g > c.b + 60);
    f.renderer->destroy(shader);
}

TEST_CASE("a custom shader the ray-traced build cannot take does not break rendering and reports why") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    // Two shaders declaring the same global: fine for raster (one shader per pipeline), a redefinition
    // once both are compiled into the ray-traced shaders.
    const char* text = "shader_type surface;\nconst float k = 1.0;\nvoid surface() { ALBEDO = vec3(k); }\n";
    const ShaderParse p = parse_ashader(text);
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "dup.ashader");
    REQUIRE(build.ok());
    const ShaderHandle a = f.renderer->create_shader(build, "a");
    const ShaderHandle b = f.renderer->create_shader(build, "b");
    REQUIRE(a.valid());
    REQUIRE(b.valid());
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle material = f.renderer->create_material({.shader = a, .params = {0, 0, 0, 0}});
    const std::array items{MeshInstance{.mesh = cube, .material = material, .transform = translate({0, 0, -2}) * scale({1, 1, 1})}};
    const RenderScene scene{.meshes = items, .sun = front_sun};
    const auto pixels = converge(f, scene, 3);
    CHECK(luminance(Fixture::center(pixels)) > 100); // still drawn (the raster stage is per shader)
    CHECK_FALSE(f.renderer->shader_error().empty());
    // Dropping one of the two fixes the ray-traced build.
    f.renderer->destroy(b);
    converge(f, scene, 2);
    CHECK(f.renderer->shader_error().empty());
    f.renderer->destroy(a);
    converge(f, scene, 2);
    CHECK(f.renderer->shader_error().empty());
}

namespace {

ShaderHandle make_shader(Fixture& f, const char* text, const char* name) {
    const ShaderParse p = parse_ashader(text);
    REQUIRE_MESSAGE(p.errors.empty(), (p.errors.empty() ? "" : p.errors[0].message));
    if (p.source.type == ShaderType::Surface) {
        const SurfaceShaderBuild build = compile_surface_shader(p.source, name);
        REQUIRE_MESSAGE(build.ok(), (build.errors.empty() ? "" : build.errors[0].message));
        return f.renderer->create_shader(build, name);
    }
    const ScreenShaderBuild build = compile_screen_shader(p.source, name);
    REQUIRE_MESSAGE(build.ok(), (build.errors.empty() ? "" : build.errors[0].message));
    return f.renderer->create_shader(build, name);
}

const char* glass_shader = R"(shader_type surface;
render_mode blend_mix;
uniform vec4 tint = vec4(0.0, 1.0, 0.0, 0.5);
void surface() { ALBEDO = vec3(0.0); EMISSION = tint.rgb; ALPHA = tint.a; }
)";

/// Opaque red wall behind, a green half-transparent pane in front of it.
void check_blending(Fixture& f, bool additive) {
    const ShaderHandle shader = make_shader(
        f, additive ? "shader_type surface;\nrender_mode blend_add;\nvoid surface() { ALBEDO = vec3(0.0); EMISSION = vec3(0.0, 1.0, 0.0); ALPHA = 0.5; }\n"
                    : glass_shader,
        "glass");
    REQUIRE(shader.valid());
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle red = f.renderer->create_material({.base_color = {1, 0, 0, 1}, .roughness = 1.0f});
    const MaterialHandle pane = f.renderer->create_material({.shader = shader, .params = {0, 1, 0, 0.5f}});
    const MeshInstance items[] = {
        {.mesh = cube, .material = red, .transform = translate({0, 0, -3}) * scale({6, 6, 0.2f})},
        {.mesh = cube, .material = pane, .transform = translate({0, 0, 0}) * scale({3, 3, 0.1f})},
    };
    const auto pixels = f.draw({.meshes = items, .sun = front_sun});
    const Rgb c = rgb(Fixture::center(pixels));
    CHECK(c.r > 50);  // the wall shows through...
    CHECK(c.g > 50);  // ...tinted by the pane
    f.renderer->destroy(shader);
}

} // namespace

TEST_CASE("blend_mix surfaces are drawn over the scene (raster)") {
    Fixture f;
    if (!f.device) return;
    check_blending(f, false);
}

TEST_CASE("blend_add surfaces add light (raster)") {
    Fixture f;
    if (!f.device) return;
    check_blending(f, true);
}

TEST_CASE("blend_mix surfaces are drawn over the scene (ray-traced)") {
    Fixture f(rt_settings());
    if (!rt_available(f)) return;
    check_blending(f, false);
}

TEST_CASE("transparent surfaces are sorted back to front and hidden behind opaque ones") {
    Fixture f;
    if (!f.device) return;
    const ShaderHandle shader = make_shader(f, glass_shader, "glass");
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle green = f.renderer->create_material({.shader = shader, .params = {0, 1, 0, 0.9f}});
    const MaterialHandle blue = f.renderer->create_material({.shader = shader, .params = {0, 0, 1, 0.9f}});
    const MaterialHandle wall = f.renderer->create_material({.base_color = {1, 1, 1, 1}, .roughness = 1.0f});
    // Submitted far-to-near and near-to-far: the nearer pane must win either way.
    for (const bool reversed : {false, true}) {
        const MeshInstance far{.mesh = cube, .material = green, .transform = translate({0, 0, -1}) * scale({3, 3, 0.1f})};
        const MeshInstance near{.mesh = cube, .material = blue, .transform = translate({0, 0, 1}) * scale({3, 3, 0.1f})};
        const MeshInstance items[] = {reversed ? near : far, reversed ? far : near};
        const auto pixels = f.draw({.meshes = items, .sun = front_sun});
        const Rgb c = rgb(Fixture::center(pixels));
        CHECK(c.b > c.g + 30);
    }
    // A pane behind an opaque wall is not seen.
    const MeshInstance hidden[] = {
        {.mesh = cube, .material = wall, .transform = translate({0, 0, 0}) * scale({6, 6, 0.2f})},
        {.mesh = cube, .material = green, .transform = translate({0, 0, -2}) * scale({3, 3, 0.1f})},
    };
    const auto pixels = f.draw({.meshes = hidden, .sun = front_sun});
    const Rgb c = rgb(Fixture::center(pixels));
    CHECK(std::abs(c.g - c.r) < 12);
    f.renderer->destroy(shader);
}

TEST_CASE("a sky shader replaces the background and leaves geometry alone") {
    Fixture f;
    if (!f.device) return;
    const ShaderHandle shader = make_shader(f, "shader_type sky;\nuniform vec3 color = vec3(0.0, 1.0, 0.0);\nvoid sky() { COLOR = color * (0.5 + 0.5 * EYEDIR.y); }\n", "sky");
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle sky = f.renderer->create_material({.shader = shader, .params = {0, 1, 0, 0}});
    const MaterialHandle red = f.renderer->create_material({.base_color = {1, 0, 0, 1}, .roughness = 1.0f});
    const MeshInstance items[] = {{.mesh = cube, .material = red, .transform = scale({1, 1, 1})}};
    const auto without = f.draw({.meshes = items, .sun = front_sun});
    const auto with = f.draw({.meshes = items, .sun = front_sun, .sky = sky});
    const Rgb corner_without = rgb(without[0]);
    const Rgb corner = rgb(with[0]);
    CHECK(corner_without.b > corner_without.g); // the default sky is bluish
    CHECK(corner.g > corner.b + 40);            // the shader's sky is green
    const Rgb c = rgb(Fixture::center(with));
    CHECK(c.r > 100); // the cube is still there
    CHECK(c.g < 60);
    f.renderer->destroy(shader);
}

TEST_CASE("post shaders run after tonemapping, in order") {
    Fixture f;
    if (!f.device) return;
    const ShaderHandle swap = make_shader(f, "shader_type post;\nvoid post() { COLOR = COLOR.bgr; }\n", "swap");
    const ShaderHandle dim = make_shader(f, "shader_type post;\nuniform float amount = 0.5;\nvoid post() { COLOR *= amount; }\n", "dim");
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle red = f.renderer->create_material({.base_color = {1, 0, 0, 1}, .roughness = 1.0f});
    const MaterialHandle swap_material = f.renderer->create_material({.shader = swap});
    const MaterialHandle dim_material = f.renderer->create_material({.shader = dim, .params = {0.5f, 0, 0, 0}});
    const MeshInstance items[] = {{.mesh = cube, .material = red, .transform = scale({2, 2, 2})}};
    const auto plain = f.draw({.meshes = items, .sun = front_sun});
    const MaterialHandle chain[] = {swap_material, dim_material};
    const auto post = f.draw({.meshes = items, .sun = front_sun, .post = chain});
    const Rgb a = rgb(Fixture::center(plain));
    const Rgb b = rgb(Fixture::center(post));
    CHECK(a.r > 100);
    CHECK(b.b > 50);              // red became blue...
    CHECK(b.b < a.r * 0.7);       // ...and was dimmed
    CHECK(b.r < 30);
    // The same edge in the UI/debug layer: post effects leave the 2D layer alone is checked in the App.
    f.renderer->destroy(swap);
    f.renderer->destroy(dim);
}

TEST_CASE("characterization: RenderStats and pass list for raster and ray-traced frames") {
    auto pass_names = [](const Renderer& r) {
        std::vector<std::string> names;
        for (const PassTiming& t : r.gpu_timings()) names.push_back(t.name);
        return names;
    };

    SUBCASE("raster frame") {
        Fixture f;
        if (!f.device) return;
        const MeshHandle cube = f.renderer->create_mesh(make_cube());
        const MeshHandle plane = f.renderer->create_mesh(make_plane(4.0f));
        const ShaderHandle glass = make_shader(f, glass_shader, "char_glass");
        const MaterialHandle red = f.renderer->create_material({.base_color = {1, 0, 0, 1}, .roughness = 0.8f});
        const MaterialHandle pane = f.renderer->create_material({.shader = glass, .params = {0, 1, 0, 0.5f}});
        const std::array meshes{
            MeshInstance{.mesh = plane, .material = red, .transform = translate({0, -1, 0})},
            MeshInstance{.mesh = cube, .material = red, .transform = translate({-1, 0, 0})},
            MeshInstance{.mesh = cube, .material = red, .transform = translate({1, 0, 0})},
            MeshInstance{.mesh = cube, .material = pane, .transform = translate({0, 0, 1})},
        };
        const std::array sdf{
            SdfShape{.primitive = SdfPrimitive::Sphere, .position = {0, 0, -2}, .size = {0.5f, 0, 0, 0}},
        };
        const std::array lights{
            PointLight{.position = {0, 2, 2}, .color = {1, 1, 1}, .intensity = 10.0f, .radius = 8.0f},
        };
        const std::array rects{
            RectLight{.position = {0, 3, 0}, .intensity = 5.0f},
        };
        DebugDraw debug;
        debug.line({-1, 0, 0}, {1, 0, 0}, {1, 1, 0, 1});
        const RenderScene scene{
            .meshes = meshes,
            .sdf = sdf,
            .lights = lights,
            .rect_lights = rects,
            .sun = {.direction = {0.4f, -1.0f, -0.3f}, .intensity = 2.0f, .shadows = true},
            .debug_lines = debug.lines(),
        };
        for (int i = 0; i < 6; ++i) f.draw(scene);

        const RenderStats& s = f.renderer->stats();
        CHECK_FALSE(s.ray_traced);
        CHECK(s.draw_calls == 3);
        CHECK(s.instances == 4);
        CHECK(s.shadow_draw_calls == 2);
        CHECK(s.sdf_shapes == 1);
        CHECK(s.point_lights == 1);
        CHECK(s.rect_lights == 0);
        CHECK(s.gi_probes == 0);
        CHECK(s.passes == 7);

        if (!f.device->supports(gpu::Feature::Timestamps)) {
            MESSAGE("no GPU timestamps — skipping raster pass list assertion");
        } else {
            const std::vector<std::string> expected{
                "shadow", "gbuffer", "sdf", "lighting", "tonemap", "transparent", "debug.lines",
            };
            CHECK(pass_names(*f.renderer) == expected);
        }
        f.renderer->destroy(glass);
    }

    SUBCASE("ray-traced frame") {
        Fixture f(rt_settings());
        if (!rt_available(f)) return;
        const MeshHandle cube = f.renderer->create_mesh(make_cube());
        const MeshHandle plane = f.renderer->create_mesh(make_plane(4.0f));
        const ShaderHandle glass = make_shader(f, glass_shader, "char_glass_rt");
        const MaterialHandle red = f.renderer->create_material({.base_color = {1, 0, 0, 1}, .roughness = 0.8f});
        const MaterialHandle pane = f.renderer->create_material({.shader = glass, .params = {0, 1, 0, 0.5f}});
        const std::array meshes{
            MeshInstance{.mesh = plane, .material = red, .transform = translate({0, -1, 0})},
            MeshInstance{.mesh = cube, .material = red, .transform = translate({-1, 0, 0})},
            MeshInstance{.mesh = cube, .material = red, .transform = translate({1, 0, 0})},
            MeshInstance{.mesh = cube, .material = pane, .transform = translate({0, 0, 1})},
        };
        const std::array sdf{
            SdfShape{.primitive = SdfPrimitive::Sphere, .position = {0, 0, -2}, .size = {0.5f, 0, 0, 0}},
        };
        const std::array lights{
            PointLight{.position = {0, 2, 2}, .color = {1, 1, 1}, .intensity = 10.0f, .radius = 8.0f},
        };
        const std::array rects{
            RectLight{.position = {0, 3, 0}, .intensity = 5.0f},
        };
        DebugDraw debug;
        debug.line({-1, 0, 0}, {1, 0, 0}, {1, 1, 0, 1});
        const RenderScene scene{
            .meshes = meshes,
            .sdf = sdf,
            .lights = lights,
            .rect_lights = rects,
            .sun = {.direction = {0.4f, -1.0f, -0.3f}, .intensity = 2.0f, .shadows = true},
            .debug_lines = debug.lines(),
        };
        for (int i = 0; i < 6; ++i) f.draw(scene);

        const RenderStats& s = f.renderer->stats();
        CHECK(s.ray_traced);
        CHECK(s.draw_calls == 3);
        CHECK(s.instances == 4);
        CHECK(s.shadow_draw_calls == 0);
        CHECK(s.sdf_shapes == 1);
        CHECK(s.point_lights == 1);
        CHECK(s.rect_lights == 1);
        CHECK(s.gi_probes == 64);
        CHECK(s.passes == 26);

        if (!f.device->supports(gpu::Feature::Timestamps)) {
            MESSAGE("no GPU timestamps — skipping ray-traced pass list assertion");
        } else {
            const std::vector<std::string> expected{
                "gbuffer", "sdf", "ddgi.trace", "ddgi.update.irradiance", "ddgi.update.distance",
                "rt.lighting", "denoise.temporal", "denoise.atrous", "denoise.atrous", "denoise.atrous",
                "composite", "taa",
                "bloom.down", "bloom.down", "bloom.down", "bloom.down", "bloom.down", "bloom.down",
                "bloom.up", "bloom.up", "bloom.up", "bloom.up", "bloom.up",
                "tonemap", "transparent", "debug.lines",
            };
            CHECK(pass_names(*f.renderer) == expected);
        }
        f.renderer->destroy(glass);
    }
}

TEST_CASE("aerial perspective tints distant opaque surfaces without changing clear foreground or sky") {
    Fixture f;
    if (!f.device) return;
    const auto cube = f.renderer->create_mesh(make_cube());
    const std::vector<Item> items{
        {.mesh = cube, .transform = translate({-0.9f, 0, 0}), .color = {1, 0, 0, 1}, .emissive = 1},
        {.mesh = cube, .transform = translate({8, 0, -32}) * scale({5, 5, 5}),
         .color = {1, 0, 0, 1}, .emissive = 1},
    };
    RenderScene scene{.meshes = f.inst(items), .sun = front_sun};
    scene.environment.sky = {0.05f, 0.22f, 0.65f};
    const auto clear = f.draw(scene);
    scene.environment.aerial_extinction = 0.15f;
    scene.environment.aerial_start_m = 8.0f;
    const auto hazy = f.draw(scene);
    const Vec3 far_position{8, 0, -30};
    const auto distant_clear = rgb(f.at(clear, far_position));
    const auto distant_hazy = rgb(f.at(hazy, far_position));
    CHECK(f.at(hazy, {-0.9f, 0, 0.5f}) == f.at(clear, {-0.9f, 0, 0.5f}));
    CHECK(hazy[0] == clear[0]);
    CHECK(distant_clear.r > distant_clear.b);
    CHECK(distant_hazy.b > distant_hazy.r);
    CHECK(distant_hazy.b > distant_clear.b);
}

TEST_CASE("froxel extinction agrees with Beer-Lambert at an opaque surface") {
    Fixture f;
    if (!f.device) return;
    const auto cube = f.renderer->create_mesh(make_cube());
    const auto material = f.renderer->create_material({.base_color = {0, 0, 0, 1}, .emissive = {1, 1, 1}});
    MeshInstance item{.mesh = cube, .material = material, .transform = Mat4::identity()};
    RenderScene scene{.meshes = std::span(&item, 1), .sun = {.intensity = 0, .shadows = false},
                      .environment = {.ambient = 0, .fog_extinction = 0.2f, .fog_range_m = 20}};
    const auto actual = rgb(Fixture::center(f.draw(scene)));
    const float expected_transmittance = std::exp(-0.2f * 4.5f);
    const auto reference = f.renderer->create_material({
        .base_color = {0, 0, 0, 1},
        .emissive = {expected_transmittance, expected_transmittance, expected_transmittance}});
    item.material = reference;
    scene.environment.fog_extinction = 0;
    const auto expected = rgb(Fixture::center(f.draw(scene)));
    CHECK(std::abs(actual.r - expected.r) <= 2);
    CHECK(std::abs(actual.g - expected.g) <= 2);
    CHECK(std::abs(actual.b - expected.b) <= 2);
}

TEST_CASE("froxel fog attenuates an additive surface at its own depth") {
    Fixture f;
    if (!f.device) return;
    const auto shader = make_shader(f,
        "shader_type surface;\nrender_mode blend_add;\nvoid surface() { ALBEDO = vec3(0); EMISSION = vec3(1, 0, 0); ALPHA = 1.0; }\n",
        "fog_additive");
    REQUIRE(shader.valid());
    const auto cube = f.renderer->create_mesh(make_cube());
    const auto pane = f.renderer->create_material({.shader = shader});
    const MeshInstance item{.mesh = cube, .material = pane, .transform = Mat4::identity()};
    RenderScene scene{.meshes = std::span(&item, 1), .sun = {.intensity = 0, .shadows = false},
                      .environment = {.sky = {}, .ground = {}, .ambient = 0}};
    const auto clear = rgb(Fixture::center(f.draw(scene)));
    scene.environment.fog_extinction = 0.65f;
    scene.environment.fog_range_m = 25.0f;
    const auto fogged = rgb(Fixture::center(f.draw(scene)));
    CHECK(clear.r > fogged.r + 30);
    CHECK(fogged.r > 0);
    CHECK(fogged.g == 0);
}

TEST_CASE("local smoke uses packed occupancy without requiring global fog") {
    Fixture f;
    if (!f.device) return;
    const auto cube = f.renderer->create_mesh(make_cube());
    const auto material = f.renderer->create_material({.base_color = {0, 0, 0, 1}, .emissive = {1, 1, 1}});
    const MeshInstance wall{.mesh = cube, .material = material,
                            .transform = translate({0, 0, -1}) * scale({6, 5, 0.2f})};
    std::array<std::uint8_t, 27> occupied{};
    occupied[13] = 1;
    SmokeVolumeView smoke{.centre = {0, 0, 1}, .cell_size_m = 1, .extinction_per_m = 2,
                          .side = 3, .occupancy = occupied};
    RenderScene scene{.meshes = std::span(&wall, 1), .sun = {.intensity = 0, .shadows = false},
                      .environment = {.ambient = 0, .fog_range_m = 16}};
    const auto clear = f.draw(scene);
    scene.smoke = std::span(&smoke, 1);
    const auto smoky = f.draw(scene);
    CHECK(rgb(Fixture::center(clear)).r > rgb(Fixture::center(smoky)).r + 60);
    CHECK(f.at(clear, {2, 0, -0.9f}) == f.at(smoky, {2, 0, -0.9f}));
    const auto shader = make_shader(f, "shader_type volume;\nuniform float mask = 1.0;\n"
                                       "void volume() { DENSITY *= mask; }\n", "smoke_mask");
    REQUIRE(shader.valid());
    smoke.material = f.renderer->create_material({.shader = shader, .params = {0.0f}});
    CHECK(f.draw(scene) == clear);
    CHECK(occupied[13] == 1);
    smoke.material = f.renderer->create_material({.shader = shader, .params = {1.0f}});
    CHECK(f.draw(scene) == smoky);
    occupied[13] = 0;
    CHECK(f.draw(scene) == clear);

    // A visual-only volume can emit light even with zero extinction and no occupied CPU cells.
    const auto emission = make_shader(f, "shader_type volume;\n"
                                         "void volume() { DENSITY = 0.0; EMISSION = vec3(0.0, AGE, 0.0); }\n",
                                      "volume_emission");
    REQUIRE(emission.valid());
    smoke.occupancy = {};
    smoke.material = {};
    CHECK(f.draw(scene) == clear);
    smoke.material = f.renderer->create_material({.shader = emission});
    CHECK(f.draw(scene) == clear);
    smoke.age = 3.0f;
    const auto glowing = rgb(Fixture::center(f.draw(scene)));
    CHECK(glowing.g > glowing.r);
}

TEST_CASE("sky IBL lights neutral diffuse and metallic surfaces using the active sky") {
    RenderSettings settings{.shadow_map_size = 512, .ray_tracing = false};
    Fixture f(settings);
    if (!f.device) return;
    const auto sky_shader = make_shader(f, "shader_type sky;\nvoid sky() { COLOR = vec3(0, 0.8, 0); }\n", "ibl_green");
    REQUIRE(sky_shader.valid());
    const auto sky = f.renderer->create_material({.shader = sky_shader});
    const auto cube = f.renderer->create_mesh(make_cube());
    const std::vector<Item> items{{.mesh = cube, .transform = translate({-0.65f, 0, 0})},
                                   {.mesh = cube, .transform = translate({0.65f, 0, 0}), .roughness = 0.4f,
                                    .metallic = 1}};
    const RenderScene scene{.meshes = f.inst(items), .sun = {.intensity = 0, .shadows = false},
                            .environment = {.sky = {0.8f, 0, 0}, .ground = {0.8f, 0, 0}, .ambient = 1}, .sky = sky};
    const auto prior = f.draw(scene);
    CHECK(rgb(f.at(prior, {-0.65f, 0, 0.5f})).r > 70);
    settings.sky_ibl = true;
    f.renderer->set_settings(settings);
    const auto lit = f.draw(scene);
    for (float x : {-0.65f, 0.65f}) {
        const auto c = rgb(f.at(lit, {x, 0, 0.5f}));
        CHECK(c.g > 70);
        CHECK(c.r == 0);
    }
}

TEST_CASE("the volume atlas allocates occupied bricks and isolates material data across volumes") {
    Fixture f;
    if (!f.device) return;
    const auto shader = make_shader(f, "shader_type volume;\nuniform vec3 tint = vec3(1);\n"
                                       "void volume() { DENSITY = 0.0; EMISSION = tint * 5.0; }\n", "brick_test");
    REQUIRE(shader.valid());
    const auto red = f.renderer->create_material({.shader = shader, .params = {1, 0, 0}});
    const auto green = f.renderer->create_material({.shader = shader, .params = {0, 1, 0}});
    constexpr unsigned side = 17;
    std::array<std::uint8_t, side * side * side> cells{};
    cells[(7 * side + 8) * side + 8] = 1;
    cells[(8 * side + 8) * side + 8] = 1;
    const std::array volumes{
        SmokeVolumeView{.centre = {-1, 0, 0}, .cell_size_m = 0.95f, .extinction_per_m = 1,
                        .side = side, .occupancy = cells, .material = red},
        SmokeVolumeView{.centre = {1, 0, 0}, .cell_size_m = 0.95f, .extinction_per_m = 1,
                        .side = side, .occupancy = cells, .material = green},
    };
    const RenderScene scene{.sun = {.intensity = 0, .shadows = false},
                            .environment = {.sky = {}, .ground = {}, .ambient = 0, .fog_range_m = 12},
                            .smoke = volumes};
    const auto pixels = f.draw(scene);
    CHECK(f.renderer->stats().volume_bricks == 4);
    const auto left = rgb(f.at(pixels, volumes[0].centre));
    const auto right = rgb(f.at(pixels, volumes[1].centre));
    CHECK(left.r > 30);
    CHECK(left.g == 0);
    CHECK(right.g > 30);
    CHECK(right.r == 0);
}

TEST_CASE("sky IBL supplies path-traced reflection and resets accumulation when the sky changes") {
    Fixture f({.shadow_map_size = 512, .ray_tracing = true, .bloom_strength = 0,
               .path_tracer = true, .path_tracer_max_bounces = 3, .sky_ibl = true});
    if (!f.device || !f.device->supports(gpu::Feature::RayTracing)) return;
    REQUIRE(f.renderer->ray_tracing());
    const auto sky_shader = make_shader(f,
        "shader_type sky;\nvoid sky() { COLOR = TIME < 0.5 ? vec3(0, 1, 0) : vec3(0, 0, 1); }\n",
        "ibl_animated");
    REQUIRE(sky_shader.valid());
    const auto sky = f.renderer->create_material({.shader = sky_shader});
    const auto cube = f.renderer->create_mesh(make_cube());
    const std::vector<Item> items{{.mesh = cube, .roughness = 0.02f, .metallic = 1}};
    RenderScene scene{.meshes = f.inst(items), .sun = {.intensity = 0, .shadows = false},
                      .environment = {.sky = {}, .ground = {}, .ambient = 0}, .sky = sky};
    const auto first = rgb(Fixture::center(f.draw(scene)));
    CHECK(first.g > 100);
    CHECK(first.b == 0);
    f.draw(scene);
    CHECK(f.renderer->stats().path_tracer_samples == 2);
    scene.time = 1;
    const auto changed = rgb(Fixture::center(f.draw(scene)));
    CHECK(f.renderer->stats().path_tracer_samples == 1);
    CHECK(changed.b > 100);
    CHECK(changed.g == 0);
}

TEST_CASE("GPU particle burst draws and obeys scene depth") {
    Fixture f;
    if (!f.device) return;
    ParticleBurst burst{.position = {0, 0, 0}, .age = 0.1f, .duration = 0.8f,
                        .color = {3, 0.4f, 0.1f}, .size_m = 0.25f};
    RenderScene scene{.sun = {.intensity = 0, .shadows = false}, .environment = {.sky = {}, .ground = {}, .ambient = 0}};
    const auto clear = f.draw(scene);
    scene.particles = std::span(&burst, 1);
    const auto lit = f.draw(scene);
    CHECK(lit != clear);
    const auto shader = make_shader(f, "shader_type particle;\nuniform float opacity = 1.0;\n"
                                       "uniform float offset = 0.0;\nvoid motion() { WORLD_POSITION = ORIGIN + vec3(offset, 0, 0); }\n"
                                       "void particle() { COLOR = vec3(0.0, 5.0, 0.0); ALPHA *= opacity; }\n",
                                    "spark_color");
    REQUIRE(shader.valid());
    burst.material = f.renderer->create_material({.shader = shader, .params = {0.0f, 0.0f}});
    CHECK(f.draw(scene) == clear);
    burst.material = f.renderer->create_material({.shader = shader, .params = {1.0f, 100.0f}});
    CHECK(f.draw(scene) == clear);
    burst.material = f.renderer->create_material({.shader = shader, .params = {1.0f, 0.0f}});
    const auto green = f.draw(scene);
    CHECK(green != clear);
    CHECK(std::equal(green.begin(), green.end(), clear.begin(),
                     [](auto effect, auto baseline) { return rgb(effect).r == rgb(baseline).r; }));
    const auto cube = f.renderer->create_mesh(make_cube());
    const std::vector<Item> floor{{.mesh = cube, .transform = translate({0, 0, -0.3f}) * scale({4, 4, 0.2f})}};
    scene.meshes = f.inst(floor);
    burst.count = 1;
    burst.age = 0.25f;
    const auto hard = rgb(Fixture::center(f.draw(scene)));
    burst.soft_distance_m = 1.0f;
    const auto soft = rgb(Fixture::center(f.draw(scene)));
    CHECK(soft.g > 0);
    CHECK(soft.g < hard.g);
    const std::vector<Item> items{{.mesh = cube, .transform = translate({0, 0, 1}) * scale({4, 4, 0.2f})}};
    scene.meshes = f.inst(items);
    const auto behind = f.draw(scene);
    scene.particles = {};
    CHECK(f.draw(scene) == behind);
}

TEST_CASE("local smoke attenuates sunlight through its own occupied cells") {
    const RenderSettings settings{.shadow_map_size = 512, .ray_tracing = false, .bloom_strength = 0};
    Fixture f(settings);
    if (!f.device) return;
    std::array<std::uint8_t, 27> occupied{};
    occupied.fill(1);
    const SmokeVolumeView smoke{.centre = {0, 0, 1}, .cell_size_m = 1, .extinction_per_m = 1.5f,
                                .side = 3, .occupancy = occupied};
    RenderScene scene{.sun = {.direction = {0, 0, 1}, .intensity = 5, .shadows = false},
                      .environment = {.sky = {}, .ground = {}, .ambient = 0, .fog_range_m = 16},
                      .smoke = std::span(&smoke, 1)};
    const auto shaded = rgb(Fixture::center(f.draw(scene)));
    auto unshadowed = settings;
    unshadowed.fog_self_shadows = false;
    f.renderer->set_settings(unshadowed);
    const auto bright = rgb(Fixture::center(f.draw(scene)));
    CHECK(shaded.r > 0);
    CHECK(bright.r > shaded.r + 20);
    scene.sun.intensity = 0;
    CHECK(rgb(Fixture::center(f.draw(scene))).r == 0);
}

TEST_CASE("GPU particle state bounces off scene depth and does not advance twice at the same age") {
    Fixture f;
    if (!f.device) return;
    f.look_from({0, 4, 6}, {0, 0, 0});
    const auto floor = f.renderer->create_mesh(make_cube());
    const std::vector<Item> items{{.mesh = floor, .transform = translate({0, -0.2f, 0}) * scale({16, 0.2f, 16}),
                                   .color = {0, 0, 0, 1}}};
    ParticleBurst burst{.color = {3, 1, 0.5f}, .size_m = 0.12f, .count = 64, .simulation_id = 1, .restitution = 0.65f};
    burst.duration = 2;
    RenderScene scene{.meshes = f.inst(items), .sun = {.intensity = 0, .shadows = false},
                      .environment = {.sky = {}, .ground = {}, .ambient = 0}, .particles = std::span(&burst, 1)};
    auto visible = [&](const std::vector<std::uint32_t>& pixels) {
        return std::count_if(pixels.begin(), pixels.end(), [](auto pixel) { return rgb(pixel).r > 15; });
    };
    for (int step = 0; step <= 12; ++step) {
        burst.age = static_cast<float>(step) * 0.1f;
        f.draw(scene);
    }
    const auto bounced = f.draw(scene);
    CHECK(f.draw(scene) == bounced);
    const auto ground = scene.meshes;
    scene.meshes = {};
    burst.simulation_id = 2;
    for (int step = 0; step <= 12; ++step) {
        burst.age = static_cast<float>(step) * 0.1f;
        f.draw(scene);
    }
    scene.meshes = ground;
    CHECK(visible(bounced) > visible(f.draw(scene)) + 10);
}

TEST_CASE("hybrid fog and lit particles receive coloured indirect light from DDGI") {
    RenderSettings settings{.shadow_map_size = 512, .ray_tracing = true,
                            .gi = {.probes_x = 3, .probes_y = 3, .probes_z = 3, .rays_per_probe = 64,
                                   .volume_min = {-2, -2, -2}, .volume_max = {2, 2, 4}},
                            .denoise_iterations = 0, .bloom_strength = 0};
    Fixture f(settings);
    if (!f.device || !f.device->supports(gpu::Feature::RayTracing)) return;
    REQUIRE(f.renderer->ray_tracing());
    const auto slab = f.renderer->create_mesh(make_cube());
    const auto material = f.renderer->create_material({.base_color = {0, 0, 0, 1}, .emissive = {0, 2, 0}});
    const MeshInstance emitter{.mesh = slab, .material = material,
                               .transform = translate({-1.5f, 0, 1}) * scale({0.25f, 5, 8})};
    RenderScene scene{.meshes = std::span(&emitter, 1), .sun = {.intensity = 0, .shadows = false},
                      .environment = {.sky = {}, .ground = {}, .ambient = 0,
                                      .fog_extinction = 0.55f, .fog_range_m = 14}};
    const auto lit = rgb(Fixture::center(f.draw(scene)));
    settings.fog_ddgi = false;
    f.renderer->set_settings(settings);
    const auto clear = rgb(Fixture::center(f.draw(scene)));
    CHECK(clear.g == 0);
    CHECK(lit.g > clear.g + 15);
    CHECK(lit.r == 0);

    const ParticleBurst burst{.position = {0, 0, 1}, .age = 0.05f, .color = {3, 3, 3},
                              .size_m = 0.26f, .lighting = 1};
    scene.environment.fog_extinction = 0;
    scene.particles = std::span(&burst, 1);
    const auto particle = rgb(Fixture::center(f.draw(scene)));
    CHECK(particle.g > 10);
    CHECK(particle.r == 0);
}
