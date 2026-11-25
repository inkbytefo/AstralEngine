// Regression tests for the Master Stabilization Roadmap (docs/status/master-stabilization-roadmap.md).
#include "frame/frame_slot.hpp"
#include "gpu_test_support.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/render/mesh.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>
#include <limits>

using namespace astral;

TEST_CASE("REN06: odd-sized mips average the whole source area (3x1 -> 1x1 keeps the last texel)") {
    ImageData img{.width = 3, .height = 1, .rgba = {}, .srgb = false};
    img.rgba = {0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255};
    const auto mips = build_mips(img);
    REQUIRE(mips.size() == 2);
    REQUIRE(mips[1].size() == 4);
    CHECK(mips[1][0] == 85); // (0 + 0 + 255) / 3
    CHECK(mips[1][3] == 255);
}

TEST_CASE("REN06: even-sized mips still match the 2x2 box filter") {
    ImageData img{.width = 2, .height = 2, .rgba = {}, .srgb = false};
    img.rgba = {0, 0, 0, 0, 100, 100, 100, 100, 200, 200, 200, 200, 100, 100, 100, 100};
    const auto mips = build_mips(img);
    REQUIRE(mips.size() == 2);
    CHECK(mips[1][0] == 100);
}

TEST_CASE("REN07: denoise iterations are clamped to the supported maximum") {
    test::TestDevice device;
    if (!device) return;
    RenderSettings s;
    s.ray_tracing = false;
    s.denoise_iterations = 1000;
    Renderer renderer(*device, gpu::Format::RGBA8Unorm, s);
    CHECK(renderer.settings().denoise_iterations == RenderSettings::max_denoise_iterations);
    s.denoise_iterations = 64;
    renderer.set_settings(s);
    CHECK(renderer.settings().denoise_iterations == RenderSettings::max_denoise_iterations);
    s.denoise_iterations = 2;
    renderer.set_settings(s);
    CHECK(renderer.settings().denoise_iterations == 2);
}

TEST_CASE("REN08: a mesh whose index names a missing vertex is refused") {
    test::TestDevice device;
    if (!device) return;
    MeshStore store(*device);
    MeshData mesh = make_cube();
    mesh.indices[1] = static_cast<std::uint32_t>(mesh.vertices.size()); // one past the end
    CHECK_FALSE(store.create(mesh, false).valid());
}

TEST_CASE("REN05: material texture offsets beyond a uniform block are refused") {
    test::TestDevice device;
    if (!device) return;
    TextureStore textures(*device);
    ShaderStore shaders(*device, gpu::Format::RGBA8Unorm);
    MaterialStore materials(*device, textures, shaders);
    MaterialDesc desc;
    desc.textures.push_back({.offset = std::numeric_limits<std::uint32_t>::max()}); // offset + 1 wraps in uint32
    CHECK_FALSE(materials.create(desc).valid());
    const MaterialHandle ok = materials.create(MaterialDesc{});
    REQUIRE(ok.valid());
    CHECK_FALSE(materials.update(ok, desc));
}

TEST_CASE("REN03: a GrowBuffer that cannot grow keeps its old buffer and retries later") {
    test::TestDevice device;
    if (!device) return;
    device.expect_errors(1); // the impossible allocation is logged
    GrowBuffer buffer;
    const std::uint32_t value = 7;
    REQUIRE(buffer.write(*device, &value, sizeof(value)));
    const gpu::BufferHandle before = buffer.buffer;
    const std::uint64_t capacity = buffer.capacity;
    CHECK_FALSE(buffer.write(*device, nullptr, std::uint64_t{1} << 62)); // cannot be allocated
    CHECK(buffer.buffer == before);
    CHECK(buffer.capacity == capacity);
    buffer.destroy(*device);
}
