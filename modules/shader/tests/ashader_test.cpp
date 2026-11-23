#include <astral/shader/ashader.hpp>

#include <doctest/doctest.h>

#include <algorithm>

using namespace astral;

namespace {

const char* kFoliage = R"(// a leaf that sways
shader_type surface;
render_mode alpha_test, cull_disabled;

uniform vec4 tint : source_color = vec4(1.0);
uniform sampler2D albedo : source_color, filter_linear_mipmap, repeat;
uniform float sway : range(0.0, 1.0, 0.01) = 0.3;
uniform vec3 wind = vec3(1, 0, 0.5);
varying float height;

float wave(float t) { return sin(t * 2.0); }

void vertex() {
    VERTEX.x += wave(TIME + WORLD_POSITION.z) * sway * UV.y;
    height = VERTEX.y;
}

void surface() {
    vec4 c = texture(albedo, UV) * tint;
    ALBEDO = c.rgb;
    ALPHA = c.a;
}
)";

} // namespace

TEST_CASE("parse_ashader reads the header and keeps the GLSL body") {
    const ShaderParse p = parse_ashader(kFoliage);
    REQUIRE_MESSAGE(p.errors.empty(), (p.errors.empty() ? "" : p.errors[0].message));
    const ShaderSource& s = p.source;
    CHECK(s.type == ShaderType::Surface);
    CHECK(s.render_modes == std::vector<std::string>{"alpha_test", "cull_disabled"});
    CHECK(s.has_render_mode("alpha_test"));
    REQUIRE(s.uniforms.size() == 4);

    const ShaderUniform& tint = s.uniforms[0];
    CHECK(tint.name == "tint");
    CHECK(tint.type == UniformType::Vec4);
    CHECK(tint.hints.source_color);
    CHECK(tint.default_value == std::vector<float>{1, 1, 1, 1}); // vec4(1.0) expands
    CHECK(tint.line == 5);

    const ShaderUniform& albedo = s.uniforms[1];
    CHECK(albedo.type == UniformType::Sampler2D);
    CHECK(albedo.hints.filter == TextureFilter::LinearMipmap);
    CHECK(albedo.hints.repeat);

    const ShaderUniform& sway = s.uniforms[2];
    CHECK(sway.type == UniformType::Float);
    REQUIRE(sway.hints.range.has_value());
    CHECK(sway.hints.range->min == doctest::Approx(0.0f));
    CHECK(sway.hints.range->max == doctest::Approx(1.0f));
    CHECK(sway.hints.range->step == doctest::Approx(0.01f));
    CHECK(sway.default_value == std::vector<float>{0.3f});

    CHECK(s.uniforms[3].default_value == std::vector<float>{1, 0, 0.5f});

    REQUIRE(s.varyings.size() == 1);
    CHECK(s.varyings[0].name == "height");
    CHECK(s.varyings[0].type == "float");

    CHECK(s.has_function("vertex"));
    CHECK(s.has_function("surface"));
    CHECK_FALSE(s.has_function("fragment"));
    // Header lines are blanked, everything else stays on its original line.
    CHECK(s.body.find("shader_type") == std::string::npos);
    CHECK(s.body.find("uniform") == std::string::npos);
    CHECK(s.body.find("float wave(float t)") != std::string::npos);
    std::size_t lines = 0;
    for (char c : s.body) lines += c == '\n';
    CHECK(lines == 22); // same line count as the file
}

TEST_CASE("parse_ashader reports problems with their line") {
    SUBCASE("missing shader_type") {
        const ShaderParse p = parse_ashader("void surface() {}\n");
        REQUIRE(p.errors.size() == 1);
        CHECK(p.errors[0].line == 1);
    }
    SUBCASE("unknown type, hint and render mode") {
        const ShaderParse p = parse_ashader("shader_type lava;\nrender_mode glow;\nuniform float x : shiny;\n");
        REQUIRE(p.errors.size() == 3);
        CHECK(p.errors[0].line == 1);
        CHECK(p.errors[1].line == 2);
        CHECK(p.errors[2].line == 3);
        CHECK(p.errors[2].message.find("shiny") != std::string::npos);
    }
    SUBCASE("unsupported uniform type and bad default") {
        const ShaderParse p = parse_ashader("shader_type surface;\nuniform mat3 m;\nuniform float f = abc;\n");
        REQUIRE(p.errors.size() == 2);
        CHECK(p.errors[0].line == 2);
        CHECK(p.errors[1].line == 3);
    }
    SUBCASE("duplicate uniform, reserved name") {
        const ShaderParse p = parse_ashader("shader_type surface;\nuniform float a;\nuniform float a;\nuniform float ALBEDO;\nuniform float astral_x;\n");
        CHECK(p.errors.size() == 3);
    }
    SUBCASE("unbalanced braces do not crash") {
        const ShaderParse p = parse_ashader("shader_type surface;\nvoid surface() { if (true) {\n");
        CHECK_FALSE(p.errors.empty());
    }
}

TEST_CASE("comments and strings in the header do not confuse the parser") {
    const ShaderParse p = parse_ashader(
        "/* shader_type sky; */ shader_type surface; // uniform float nope;\n"
        "uniform float a /* the a */ = 2.0; // done\n"
        "void surface() { /* } */ ALBEDO = vec3(a); }\n");
    REQUIRE(p.errors.empty());
    CHECK(p.source.type == ShaderType::Surface);
    REQUIRE(p.source.uniforms.size() == 1);
    CHECK(p.source.uniforms[0].default_value == std::vector<float>{2.0f});
    CHECK(p.source.has_function("surface"));
}

TEST_CASE("uniform layout packs scalars and vectors with std430 alignment") {
    const ShaderParse p = parse_ashader(
        "shader_type surface;\n"
        "uniform float a;\nuniform vec3 b;\nuniform float c;\nuniform vec2 d;\nuniform sampler2D t;\nuniform vec4 e;\n");
    REQUIRE(p.errors.empty());
    const UniformLayout layout = layout_uniforms(p.source);
    // offsets in 4-byte words: a 0; b aligned 4 (vec3 like vec4 alignment); c 7 (packs after vec3);
    // d aligned 8; t 10; e aligned 12. Total 16 words.
    CHECK(layout.offsets == std::vector<std::uint32_t>{0, 4, 7, 8, 10, 12});
    CHECK(layout.words == 16);
}

TEST_CASE("body_without blanks one function and keeps the lines") {
    const ShaderParse p = parse_ashader(kFoliage);
    REQUIRE(p.errors.empty());
    const std::string vertex_only = p.source.body_without("surface");
    CHECK(vertex_only.find("ALBEDO") == std::string::npos);
    CHECK(vertex_only.find("VERTEX.x") != std::string::npos);
    CHECK(std::count(vertex_only.begin(), vertex_only.end(), '\n') == 22);
    CHECK(p.source.body_without("nope") == p.source.body);
}

TEST_CASE("parse_ashader parses shading_model for surface shaders and rejects invalid ones") {
    for (std::string_view m : known_shading_models()) {
        const std::string src = "shader_type surface;\nshading_model " + std::string(m) + ";\nvoid surface() { ALBEDO = vec3(1.0); }\n";
        const ShaderParse p = parse_ashader(src);
        REQUIRE_MESSAGE(p.errors.empty(), (p.errors.empty() ? "" : p.errors[0].message));
        CHECK(to_string(p.source.shading_model) == m);
        CHECK(p.source.body.find("shading_model") == std::string::npos);
    }
    const ShaderParse bad_model = parse_ashader("shader_type surface;\nshading_model toon;\nvoid surface() {}\n");
    CHECK_FALSE(bad_model.errors.empty());
    const ShaderParse bad_type = parse_ashader("shader_type sky;\nshading_model clearcoat;\nvoid sky() {}\n");
    CHECK_FALSE(bad_type.errors.empty());
}
