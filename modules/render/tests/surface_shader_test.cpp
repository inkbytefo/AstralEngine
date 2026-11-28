#include <astral/core/jobs.hpp>
#include <astral/render/surface_shader.hpp>
#include <astral/shader/spirv_cache.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <string>

using namespace astral;

namespace {

const char* kLeaf = R"(shader_type surface;
render_mode alpha_test, cull_disabled;
uniform vec4 tint : source_color = vec4(1.0);
uniform sampler2D albedo : source_color;
uniform float sway : range(0.0, 1.0) = 0.3;
varying float height;

float wave(float t) { return sin(t * 2.0); }

void vertex() {
    VERTEX.x += wave(TIME + WORLD_POSITION.z) * sway * UV.y;
    height = VERTEX.y;
}

void surface() {
    vec4 c = texture(albedo, UV) * tint;
    ALBEDO = c.rgb * (0.5 + height);
    ALPHA = c.a;
    NORMAL_MAP = vec3(0.5, 0.5, 1.0);
    EMISSION = vec3(dFdx(UV.x));
}
)";

} // namespace

TEST_CASE("a surface shader compiles to G-buffer and shadow stages") {
    const ShaderParse p = parse_ashader(kLeaf);
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "leaf.ashader");
    REQUIRE_MESSAGE(build.ok(), (build.errors.empty() ? std::string() : build.errors[0].message));
    CHECK_FALSE(build.gbuffer_vertex.empty());
    CHECK_FALSE(build.gbuffer_fragment.empty());
    CHECK_FALSE(build.shadow_vertex.empty());
    CHECK_FALSE(build.shadow_fragment.empty()); // alpha_test: shadows discard too
    CHECK(build.cull_disabled);
    CHECK(build.layout.words == 8);
}

TEST_CASE("a shader without vertex() and alpha test needs no shadow fragment") {
    const ShaderParse p = parse_ashader("shader_type surface;\nvoid surface() { ALBEDO = vec3(1.0, 0.0, 0.0); }\n");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "red.ashader");
    REQUIRE(build.ok());
    CHECK(build.shadow_fragment.empty());
}

TEST_CASE("errors in the body point at the .ashader line") {
    const ShaderParse p = parse_ashader("shader_type surface;\n\nvoid surface() {\n    ALBEDO = vec3(oops);\n}\n");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "bad.ashader");
    CHECK_FALSE(build.ok());
    REQUIRE_FALSE(build.errors.empty());
    CHECK(build.errors[0].line == 4);
    CHECK(build.errors[0].message.find("oops") != std::string::npos);
}

TEST_CASE("only surface shaders are built here") {
    const ShaderParse p = parse_ashader("shader_type sky;\nvoid sky() {}\n");
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "sky.ashader");
    CHECK_FALSE(build.ok());
}

TEST_CASE("uniforms may have the names of the engine's own variables") {
    const ShaderParse p = parse_ashader(R"(shader_type surface;
uniform float scale = 2.0;
uniform vec3 n = vec3(0.0, 1.0, 0.0);
uniform float f = 1.0;
uniform float s = 1.0;
uniform float base = 0.5;
uniform vec3 uv = vec3(1.0);
uniform sampler2D map : hint_normal;
void surface() { ALBEDO = n * scale * f * s * base * uv; NORMAL_MAP = texture(map, UV).xyz; }
)");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "names.ashader");
    CHECK_MESSAGE(build.ok(), (build.errors.empty() ? std::string() : build.errors[0].message));
    // The ray-traced variant is built from the same text: it must compile with the engine's code too.
    const RtSurface rt[] = {{3, build.rt_chunk}};
    const RtVariants variants = compile_rt_variants(rt);
    CHECK_MESSAGE(variants.ok(), (variants.errors.empty() ? std::string() : variants.errors[0].message));
}

namespace {

struct CacheDir {
    std::filesystem::path path;
    explicit CacheDir(const char* tag) {
        path = std::filesystem::temp_directory_path() /
               (std::string("astral_surface_cache_") + tag + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    }
    ~CacheDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

void check_same_build(const SurfaceShaderBuild& a, const SurfaceShaderBuild& b) {
    CHECK(a.gbuffer_vertex == b.gbuffer_vertex);
    CHECK(a.gbuffer_fragment == b.gbuffer_fragment);
    CHECK(a.shadow_vertex == b.shadow_vertex);
    CHECK(a.shadow_fragment == b.shadow_fragment);
    CHECK(a.forward_vertex == b.forward_vertex);
    CHECK(a.forward_fragment == b.forward_fragment);
    CHECK(a.rt_chunk == b.rt_chunk);
}

} // namespace

TEST_CASE("a surface shader compiled with jobs and a cache equals the serial uncached build") {
    const ShaderParse p = parse_ashader(kLeaf);
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild serial = compile_surface_shader(p.source, "leaf.ashader");
    REQUIRE(serial.ok());

    CacheDir dir("surface");
    JobSystem jobs(4);
    SpirvCache cold_cache(dir.path);
    const SurfaceShaderBuild cold = compile_surface_shader(p.source, "leaf.ashader", {.cache = &cold_cache, .jobs = &jobs});
    REQUIRE(cold.ok());
    check_same_build(cold, serial);
    const std::uint64_t stages = cold_cache.stats().stores;
    CHECK(stages == 4); // g-buffer fragment + vertex, shadow vertex + fragment (alpha_test)
    CHECK(cold_cache.stats().hits == 0);

    SpirvCache warm_cache(dir.path); // the next launch
    const SurfaceShaderBuild warm = compile_surface_shader(p.source, "leaf.ashader", {.cache = &warm_cache, .jobs = &jobs});
    REQUIRE(warm.ok());
    check_same_build(warm, serial);
    CHECK(warm_cache.stats().hits == stages);   // every stage came from disk: glslang never ran
    CHECK(warm_cache.stats().misses == 0);
}

TEST_CASE("an edited surface shader misses the cache; a blend and a screen shader cache too") {
    CacheDir dir("variants");
    SpirvCache cache(dir.path);
    const ShaderCompileServices services{.cache = &cache};
    const ShaderParse leaf = parse_ashader(kLeaf);
    REQUIRE(compile_surface_shader(leaf.source, "a.ashader", services).ok());
    const std::uint64_t first = cache.stats().stores;
    const ShaderParse edited = parse_ashader(std::string(kLeaf) + "\n// edited\n");
    REQUIRE(edited.errors.empty());
    REQUIRE(compile_surface_shader(edited.source, "a.ashader", services).ok());
    CHECK(cache.stats().stores == 2 * first); // the changed text is a new entry, never the old one
    CHECK(cache.stats().hits == 0);

    const ShaderParse blend = parse_ashader("shader_type surface;\nrender_mode blend_mix;\nvoid surface() { ALBEDO = vec3(1.0); ALPHA = 0.5; }\n");
    REQUIRE(blend.errors.empty());
    const SurfaceShaderBuild serial_blend = compile_surface_shader(blend.source, "blend.ashader");
    REQUIRE(serial_blend.ok());
    const SurfaceShaderBuild cached_blend = compile_surface_shader(blend.source, "blend.ashader", services);
    check_same_build(cached_blend, serial_blend);
    const std::uint64_t hits_before = cache.stats().hits;
    CHECK(compile_surface_shader(blend.source, "blend.ashader", services).ok());
    CHECK(cache.stats().hits == hits_before + 2); // forward fragment + vertex

    const ShaderParse sky = parse_ashader("shader_type sky;\nvoid sky() { COLOR = vec3(0.1, 0.2, 0.4); }\n");
    REQUIRE(sky.errors.empty());
    const ScreenShaderBuild serial_sky = compile_screen_shader(sky.source, "sky.ashader");
    REQUIRE(serial_sky.ok());
    const ScreenShaderBuild cached_sky = compile_screen_shader(sky.source, "sky.ashader", services);
    CHECK(cached_sky.fragment == serial_sky.fragment);
    CHECK(cached_sky.vertex == serial_sky.vertex);
    CHECK(cached_sky.sky_capture == serial_sky.sky_capture);
}

TEST_CASE("the ray-traced variants compile in parallel and warm from the cache") {
    const ShaderParse p = parse_ashader("shader_type surface;\nvoid surface() { ALBEDO = vec3(0.2, 0.4, 0.6); }\n");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "rt.ashader");
    REQUIRE(build.ok());
    const RtSurface rt[] = {{3, build.rt_chunk}};
    const RtVariants serial = compile_rt_variants(rt);
    REQUIRE(serial.ok());

    CacheDir dir("rt");
    JobSystem jobs(4);
    SpirvCache cold_cache(dir.path);
    const RtVariants cold = compile_rt_variants(rt, {.cache = &cold_cache, .jobs = &jobs});
    REQUIRE(cold.ok());
    CHECK(cold.lighting == serial.lighting);
    CHECK(cold.ddgi_trace == serial.ddgi_trace);
    CHECK(cold.restir_temporal == serial.restir_temporal);
    CHECK(cold.restir_spatial == serial.restir_spatial);
    CHECK(cold.path_tracer == serial.path_tracer);
    CHECK(cold_cache.stats().stores == 5);

    SpirvCache warm_cache(dir.path);
    const RtVariants warm = compile_rt_variants(rt, {.cache = &warm_cache, .jobs = &jobs});
    REQUIRE(warm.ok());
    CHECK(warm_cache.stats().hits == 5);
    CHECK(warm_cache.stats().misses == 0);
    CHECK(warm.path_tracer == serial.path_tracer);
    CHECK(warm.lighting == serial.lighting);

    // A different set of custom surfaces is a different shader text, so it must not reuse these entries.
    const RtSurface other[] = {{3, build.rt_chunk}, {4, build.rt_chunk}};
    SpirvCache other_cache(dir.path);
    REQUIRE(compile_rt_variants(other, {.cache = &other_cache, .jobs = &jobs}).ok());
    CHECK(other_cache.stats().misses == 5);
}

TEST_CASE("the layer library is available to surface() in the raster stages and the ray-traced variants") {
    const ShaderParse p = parse_ashader(R"(shader_type surface;
uniform float coverage : range(0.0, 1.0) = 0.5;
void surface() {
    AstralLayer a = astral_layer_default();
    AstralLayer b = astral_layer_default();
    b.albedo = vec3(0.0, 1.0, 0.0);
    b.height = 0.9;
    float m = coverage * astral_mask_slope(NORMAL, 0.5, 0.9) * astral_mask_height(WORLD_POSITION, 0.0, 2.0);
    m = astral_mask_contrast(m + 0.1 * astral_mask_noise(WORLD_POSITION * 3.0), 0.4);
    AstralLayer l = astral_layer_blend_height(a, b, m, 0.2);
    ALBEDO = l.albedo; ROUGHNESS = l.roughness; METALLIC = l.metallic; EMISSION = l.emission; AO = l.ao;
    NORMAL_MAP = l.normal_map;
}
)");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "layers.ashader");
    REQUIRE_MESSAGE(build.ok(), (build.errors.empty() ? "" : build.errors[0].message));
    REQUIRE_FALSE(build.rt_chunk.empty());
    const RtSurface rt[] = {{1, build.rt_chunk}};
    const RtVariants variants = compile_rt_variants(rt);
    CHECK_MESSAGE(variants.ok(), (variants.errors.empty() ? "" : variants.errors[0].message));
}
