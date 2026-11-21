#include "gpu_test_support.hpp"

#include <astral/render/renderer.hpp>

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
