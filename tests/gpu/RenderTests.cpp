#include <doctest/doctest.h>

#include "GpuTestSupport.hpp"
#include "Astral/Render/MeshPrimitives.hpp"
#include "Astral/Render/Renderer.hpp"

#include <cstdio>
#include <cstdlib>

using namespace Astral::Render;
using GpuTest::PixelAt;
using GpuTest::RenderAndPick;

namespace {

constexpr uint32_t kW = 320, kH = 180;

Camera LookingDownMinusZ(float z = 5.0f) {
    Camera c;
    c.position = {0.0f, 0.0f, z};
    return c;
}

uint32_t PickCentre(Renderer& r, ViewHandle view, const Camera& cam) {
    auto pick = RenderAndPick(r, view, cam, kW / 2, kH / 2);
    REQUIRE(pick.has_value());
    return pick->objectId;
}

} // namespace

TEST_CASE("An empty world renders the sky and reports no SDF memory") {
    Renderer r;
    ViewHandle view = r.CreateView(kW, kH);
    REQUIRE(r.BeginFrame());
    r.RenderView(view, LookingDownMinusZ());
    r.EndFrame();
    auto pixels = r.ReadViewPixels(view);
    REQUIRE(pixels.size() == size_t(kW) * kH * 4);
    auto sky = PixelAt(pixels, kW, kW / 2, 5);
    CHECK(sky.b > sky.r); // bluish sky
    CHECK(r.GetStats().sdfBrickCapacity <= 1); // the SDF volume is not allocated without SDF shapes
}

TEST_CASE("Meshes and SDF shapes are depth tested against each other") {
    Renderer r;
    World& world = r.GetWorld();
    ViewHandle view = r.CreateView(kW, kH);

    // An SDF sphere at the origin and a mesh cube in front of it, both centred on screen.
    SdfShapeDesc sphere;
    sphere.size = glm::vec4(1.0f);
    sphere.objectId = 20;
    world.CreateSdfShape(sphere);
    MeshHandle cube = r.CreateMesh(Primitives::Cube(0.4f));
    InstanceHandle inst = world.CreateInstance(cube, r.GetDefaultMaterial(), glm::translate(glm::mat4(1.0f), {0, 0, 2.0f}), 10);

    CHECK(PickCentre(r, view, LookingDownMinusZ()) == 10); // mesh in front

    world.SetInstanceTransform(inst, glm::translate(glm::mat4(1.0f), {0, 0, -2.0f}));
    CHECK(PickCentre(r, view, LookingDownMinusZ()) == 20); // mesh behind the SDF

    world.DestroyInstance(inst);
    world.CreateSdfShape([] {
        SdfShapeDesc carve;
        carve.operation = SdfOperation::Subtract;
        carve.position = {0, 0, 1.0f};
        carve.size = glm::vec4(0.6f);
        return carve;
    }());
    auto pick = RenderAndPick(r, view, LookingDownMinusZ(), kW / 2, kH / 2);
    REQUIRE(pick);
    CHECK(pick->objectId == 20);
    CHECK(pick->worldPosition.z < 0.6f); // the subtraction carved a hole: the hit is deeper
    CHECK(r.GetStats().sdfBricksUsed > 0);
}

TEST_CASE("Editing one SDF shape re-bakes only the cells around it") {
    Renderer r;
    World& world = r.GetWorld();
    ViewHandle view = r.CreateView(kW, kH);
    SdfShapeDesc a;
    a.position = {-10, 0, 0};
    SdfShapeHandle ha = world.CreateSdfShape(a);
    SdfShapeDesc b;
    b.position = {10, 0, 0};
    world.CreateSdfShape(b);

    for (int i = 0; i < 2; ++i) {
        REQUIRE(r.BeginFrame());
        r.RenderView(view, LookingDownMinusZ());
        r.EndFrame();
    }
    const uint32_t full = r.GetStats().sdfLastBakeCells;

    a.position.y += 0.5f;
    world.UpdateSdfShape(ha, a);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(r.BeginFrame());
        r.RenderView(view, LookingDownMinusZ());
        r.EndFrame();
    }
    r.WaitIdle();
    const uint32_t partial = r.GetStats().sdfLastBakeCells;
    CHECK(partial > 0);
    CHECK(partial * 50 < full);
}

TEST_CASE("Every debug view renders") {
    Renderer r;
    World& world = r.GetWorld();
    world.CreateSdfShape(SdfShapeDesc{});
    world.CreateInstance(r.CreateMesh(Primitives::Sphere(0.5f)), r.GetDefaultMaterial(),
                         glm::translate(glm::mat4(1.0f), {1.5f, 0, 0}), 3);
    LightDesc sun;
    sun.type = LightType::Directional;
    sun.direction = glm::normalize(glm::vec3(-1, -1, -1));
    world.CreateLight(sun);
    ViewHandle view = r.CreateView(kW, kH);
    for (uint32_t d = 0; d <= uint32_t(DebugView::SdfSteps); ++d) {
        ViewSettings s;
        s.debugView = DebugView(d);
        REQUIRE(r.BeginFrame());
        r.RenderView(view, LookingDownMinusZ(), s);
        r.EndFrame();
    }
    r.WaitIdle();
    CHECK(GpuTest::g_VulkanErrors == 0);
}

TEST_CASE("Textures and materials can be created, used and destroyed") {
    Renderer r;
    std::vector<uint8_t> checker(64 * 64 * 4);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            uint8_t v = ((x / 8 + y / 8) % 2) ? 255 : 30;
            for (int c = 0; c < 3; ++c) checker[(y * 64 + x) * 4 + c] = v;
            checker[(y * 64 + x) * 4 + 3] = 255;
        }
    TextureDesc td;
    td.width = td.height = 64;
    td.pixels = checker;
    TextureHandle tex = r.CreateTexture(td);
    MaterialDesc md;
    md.baseColorTexture = tex;
    MaterialHandle mat = r.CreateMaterial(md);
    r.GetWorld().CreateInstance(r.CreateMesh(Primitives::Plane(2.0f)), mat,
                                glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0)), 1);
    ViewHandle view = r.CreateView(kW, kH);
    REQUIRE(r.BeginFrame());
    r.RenderView(view, LookingDownMinusZ(3.0f));
    r.EndFrame();
    auto pixels = r.ReadViewPixels(view);
    // The checker pattern shows up as strong contrast across the plane.
    int lo = 255, hi = 0;
    for (uint32_t y = kH / 2 - 30; y < kH / 2 + 30; ++y)
        for (uint32_t x = kW / 2 - 40; x < kW / 2 + 40; ++x) {
            auto p = PixelAt(pixels, kW, x, y);
            lo = std::min(lo, p.g);
            hi = std::max(hi, p.g);
        }
    if (const char* dump = std::getenv("ASTRAL_TEST_DUMP")) {
        FILE* f = std::fopen(dump, "wb");
        std::fprintf(f, "P6 %u %u 255\n", kW, kH);
        for (size_t i = 0; i < pixels.size(); i += 4) std::fwrite(&pixels[i], 1, 3, f);
        std::fclose(f);
    }
    CHECK(hi - lo > 60);
    r.DestroyMaterial(mat);
    r.DestroyTexture(tex);
}

TEST_CASE("Smooth unions stay continuous where earlier shapes are culled") {
    // A big sphere followed by a small smooth-unioned bump: around the bump the big sphere is far
    // from the surface but still shapes it through the blend. Culling it there used to tear the field.
    Renderer r;
    World& world = r.GetWorld();
    SdfShapeDesc hill;
    hill.size = glm::vec4(4.0f);
    hill.position = {0.0f, -2.2f, -1.0f};
    world.CreateSdfShape(hill);
    SdfShapeDesc bump;
    bump.operation = SdfOperation::SmoothUnion;
    bump.blend = 1.2f;
    bump.size = glm::vec4(1.8f);
    bump.position = {0.0f, -0.8f, 1.0f};
    bump.order = 1;
    world.CreateSdfShape(bump);

    Camera cam;
    cam.position = {0.0f, 1.5f, 8.0f};
    cam.rotation = glm::angleAxis(glm::radians(-12.0f), glm::vec3(1, 0, 0));
    ViewHandle view = r.CreateView(kW, kH);
    for (uint32_t y = kH / 2 - 20; y <= kH / 2 + 20; y += 20) {
        auto pick = RenderAndPick(r, view, cam, kW / 2, y);
        REQUIRE(pick);
        CHECK(std::abs(world.SdfDistance(pick->worldPosition)) < 0.03f);
    }
}
