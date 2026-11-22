#include <astral/render/surface_shader.hpp>

#include <doctest/doctest.h>

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
