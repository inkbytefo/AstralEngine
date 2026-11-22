#include <astral/shader/ashader.hpp>
#include <doctest/doctest.h>

#include <string>
#include <string_view>

using namespace astral;

namespace {

std::string to_crlf(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 64);
    for (char c : text) {
        if (c == '\n') out += "\r\n";
        else if (c != '\r') out += c;
    }
    return out;
}

} // namespace

TEST_CASE("parse_ashader accepts CRLF line endings identically to LF") {
    const std::string lf =
        "// a leaf that sways\n"
        "shader_type surface;\n"
        "render_mode alpha_test, cull_disabled;\n"
        "\n"
        "/* multi-line\n"
        "   comment */\n"
        "uniform vec4 tint : source_color = vec4(1.0);\n"
        "uniform sampler2D albedo : source_color, filter_linear_mipmap, repeat;\n"
        "uniform float sway : range(0.0, 1.0, 0.01) = 0.3;\n"
        "varying float height;\n"
        "\n"
        "float wave(float t) {\n"
        "    return sin(t * 2.0);\n"
        "}\n"
        "\n"
        "void vertex() {\n"
        "    VERTEX.x += wave(TIME + WORLD_POSITION.z) * sway * UV.y;\n"
        "    height = VERTEX.y;\n"
        "}\n"
        "\n"
        "void surface() {\n"
        "    vec4 c = texture(albedo, UV) * tint;\n"
        "    ALBEDO = c.rgb;\n"
        "    ALPHA = c.a;\n"
        "}\n";
    const std::string crlf = to_crlf(lf);

    const ShaderParse parsed_lf = parse_ashader(lf);
    const ShaderParse parsed_crlf = parse_ashader(crlf);

    REQUIRE_MESSAGE(parsed_lf.errors.empty(), (parsed_lf.errors.empty() ? "" : parsed_lf.errors[0].message));
    REQUIRE_MESSAGE(parsed_crlf.errors.empty(), (parsed_crlf.errors.empty() ? "" : parsed_crlf.errors[0].message));

    const ShaderSource& a = parsed_lf.source;
    const ShaderSource& b = parsed_crlf.source;
    CHECK(b.type == a.type);
    CHECK(b.render_modes == a.render_modes);
    CHECK(b.functions == a.functions);
    CHECK(b.function_ranges == a.function_ranges);
    CHECK(b.body == a.body);
    CHECK(b.body_without("surface") == a.body_without("surface"));
    CHECK(b.body_without("vertex") == a.body_without("vertex"));

    REQUIRE(b.uniforms.size() == a.uniforms.size());
    for (std::size_t i = 0; i < a.uniforms.size(); ++i) {
        CHECK(b.uniforms[i].name == a.uniforms[i].name);
        CHECK(b.uniforms[i].type == a.uniforms[i].type);
        CHECK(b.uniforms[i].line == a.uniforms[i].line);
        CHECK(b.uniforms[i].default_value == a.uniforms[i].default_value);
        CHECK(b.uniforms[i].hints.source_color == a.uniforms[i].hints.source_color);
        CHECK(b.uniforms[i].hints.hint_normal == a.uniforms[i].hints.hint_normal);
        CHECK(b.uniforms[i].hints.filter == a.uniforms[i].hints.filter);
        CHECK(b.uniforms[i].hints.repeat == a.uniforms[i].hints.repeat);
        CHECK(b.uniforms[i].hints.range.has_value() == a.uniforms[i].hints.range.has_value());
    }

    REQUIRE(b.varyings.size() == a.varyings.size());
    for (std::size_t i = 0; i < a.varyings.size(); ++i) {
        CHECK(b.varyings[i].type == a.varyings[i].type);
        CHECK(b.varyings[i].name == a.varyings[i].name);
    }

    const UniformLayout layout_a = layout_uniforms(a);
    const UniformLayout layout_b = layout_uniforms(b);
    CHECK(layout_b.offsets == layout_a.offsets);
    CHECK(layout_b.words == layout_a.words);
}
