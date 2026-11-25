// Regression tests for the Master Stabilization Roadmap (docs/archive/msr/master-stabilization-roadmap.md).
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

// ---- AS13: typed shader parameter words (ADR-0042) ----
#include <astral/render/material_file.hpp>
#include <astral/shader/ashader.hpp>

#include <bit>
#include <string>

namespace astral::detail {
std::string uniform_macros(const ShaderSource& source, const UniformLayout& layout);
}

namespace {
ShaderSource as13_shader(const char* uniforms) {
    const std::string text = std::string("shader_type surface;\n") + uniforms + "\nvoid surface() {}\n";
    ShaderParse p = parse_ashader(text);
    CAPTURE(text);
    REQUIRE(p.errors.empty());
    return std::move(p.source);
}
MaterialFile as13_material(const char* values_json) {
    MaterialFile m;
    m.shader = "x.ashader";
    const auto doc = Json::parse(values_json);
    REQUIRE(doc.has_value());
    for (const auto& [name, v] : doc->as_object()) m.values.emplace_back(name, v);
    return m;
}
} // namespace

TEST_CASE("AS13: int uniforms keep the whole int32 range (defaults and overrides)") {
    const ShaderSource s = as13_shader("uniform int big = 16777217;\nuniform int low = -2147483648;\nuniform int high = 2147483647;\n"
                                       "uniform float f = 0.1;\nuniform bool on = true;\nuniform vec3 c = vec3(1.0, 2.0, 3.0);");
    const UniformLayout layout = layout_uniforms(s);
    const UniformValues defaults = evaluate_uniforms(s, layout, MaterialFile{});
    CHECK(defaults.warnings.empty());
    CHECK(defaults.params[layout.offsets[0]] == 16777217u);
    CHECK(std::bit_cast<std::int32_t>(defaults.params[layout.offsets[1]]) == std::numeric_limits<std::int32_t>::min());
    CHECK(std::bit_cast<std::int32_t>(defaults.params[layout.offsets[2]]) == std::numeric_limits<std::int32_t>::max());
    CHECK(defaults.params[layout.offsets[3]] == std::bit_cast<std::uint32_t>(0.1f));
    CHECK(defaults.params[layout.offsets[4]] == 1u);
    CHECK(defaults.params[layout.offsets[5] + 2] == std::bit_cast<std::uint32_t>(3.0f));

    const UniformValues given = evaluate_uniforms(s, layout, as13_material(R"({"big": 16777219, "low": -5, "on": false})"));
    CHECK(given.warnings.empty());
    CHECK(given.params[layout.offsets[0]] == 16777219u);
    CHECK(std::bit_cast<std::int32_t>(given.params[layout.offsets[1]]) == -5);
    CHECK(given.params[layout.offsets[4]] == 0u);
}

TEST_CASE("AS13: int values that are fractional, out of range or not finite are refused") {
    CHECK_FALSE(parse_ashader("shader_type surface;\nuniform int a = 1.5;\nvoid surface() {}\n").errors.empty());
    CHECK_FALSE(parse_ashader("shader_type surface;\nuniform int a = 2147483648;\nvoid surface() {}\n").errors.empty());
    CHECK_FALSE(parse_ashader("shader_type surface;\nuniform int a = inf;\nvoid surface() {}\n").errors.empty());
    const ShaderSource s = as13_shader("uniform int a = 7;");
    const UniformLayout layout = layout_uniforms(s);
    for (const char* bad : {R"({"a": 2.5})", R"({"a": 3000000000})", R"({"a": -2147483649})"}) {
        CAPTURE(bad);
        const UniformValues v = evaluate_uniforms(s, layout, as13_material(bad));
        CHECK(v.warnings.size() == 1);
        CHECK(v.params[layout.offsets[0]] == 7u); // the default is kept
    }
}

TEST_CASE("AS13: generated GLSL reads ints and bools from the uint words, not through float") {
    const ShaderSource s = as13_shader("uniform int n = 1;\nuniform bool b = false;\nuniform float f = 0.0;");
    const std::string macros = detail::uniform_macros(s, layout_uniforms(s));
    CHECK(macros.find("int(astral_pu(") != std::string::npos);
    CHECK(macros.find("(astral_pu(") != std::string::npos);
    CHECK(macros.find("int(astral_pf(") == std::string::npos);
    CHECK(macros.find("astral_pf(") != std::string::npos); // floats are unchanged
}
