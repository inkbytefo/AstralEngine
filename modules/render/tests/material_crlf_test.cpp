#include <astral/render/material_file.hpp>
#include <doctest/doctest.h>

#include <map>
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

MaterialReader map_reader(const std::map<std::string, std::string>& files) {
    return [&files](std::string_view path, std::string* error) -> std::optional<Json> {
        auto it = files.find(std::string(path));
        if (it == files.end()) {
            if (error) *error = "no such file";
            return std::nullopt;
        }
        return Json::parse(it->second, error);
    };
}

} // namespace

TEST_CASE("material files with CRLF line endings resolve identically to LF") {
    const std::string base_lf =
        "{\n"
        "  \"format\": \"astral.material\",\n"
        "  \"version\": 1,\n"
        "  \"shader\": \"shaders/leaf.ashader\",\n"
        "  \"values\": {\n"
        "    \"tint\": [1, 0.5, 0.25, 1],\n"
        "    \"sway\": 0.2,\n"
        "    \"albedo\": \"textures/leaf.png\"\n"
        "  }\n"
        "}\n";
    const std::string child_lf =
        "{\n"
        "  \"format\": \"astral.material\",\n"
        "  \"version\": 1,\n"
        "  \"parent\": \"materials/base.material\",\n"
        "  \"values\": {\n"
        "    \"sway\": 0.75\n"
        "  }\n"
        "}\n";

    std::map<std::string, std::string> lf_files{
        {"materials/base.material", base_lf},
        {"materials/windy.material", child_lf},
    };
    std::map<std::string, std::string> crlf_files{
        {"materials/base.material", to_crlf(base_lf)},
        {"materials/windy.material", to_crlf(child_lf)},
    };

    const MaterialFile mat_lf = resolve_material("materials/windy.material", map_reader(lf_files));
    const MaterialFile mat_crlf = resolve_material("materials/windy.material", map_reader(crlf_files));

    REQUIRE_MESSAGE(mat_lf.ok(), (mat_lf.errors.empty() ? "" : mat_lf.errors[0]));
    REQUIRE_MESSAGE(mat_crlf.ok(), (mat_crlf.errors.empty() ? "" : mat_crlf.errors[0]));
    CHECK(mat_crlf.shader == mat_lf.shader);
    CHECK(mat_crlf.dependencies == mat_lf.dependencies);
    CHECK(mat_crlf.values == mat_lf.values);

    const std::string shader_lf =
        "shader_type surface;\n"
        "uniform vec4 tint : source_color = vec4(1.0);\n"
        "uniform float sway = 0.1;\n"
        "uniform sampler2D albedo : source_color;\n"
        "void surface() {}\n";
    const ShaderParse sh_lf = parse_ashader(shader_lf);
    const ShaderParse sh_crlf = parse_ashader(to_crlf(shader_lf));
    REQUIRE(sh_lf.errors.empty());
    REQUIRE(sh_crlf.errors.empty());

    const UniformValues eval_lf = evaluate_uniforms(sh_lf.source, layout_uniforms(sh_lf.source), mat_lf);
    const UniformValues eval_crlf = evaluate_uniforms(sh_crlf.source, layout_uniforms(sh_crlf.source), mat_crlf);
    CHECK(eval_crlf.params == eval_lf.params);
    CHECK(eval_crlf.warnings == eval_lf.warnings);
    REQUIRE(eval_crlf.textures.size() == eval_lf.textures.size());
    for (std::size_t i = 0; i < eval_lf.textures.size(); ++i) {
        CHECK(eval_crlf.textures[i].offset == eval_lf.textures[i].offset);
        CHECK(eval_crlf.textures[i].path == eval_lf.textures[i].path);
        CHECK(eval_crlf.textures[i].srgb == eval_lf.textures[i].srgb);
        CHECK(eval_crlf.textures[i].normal == eval_lf.textures[i].normal);
    }
}
