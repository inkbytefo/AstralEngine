#include <astral/render/material_file.hpp>

#include <doctest/doctest.h>

#include <bit>
#include <map>

using namespace astral;

namespace {

struct Files {
    std::map<std::string, std::string> text;
    MaterialReader reader() const {
        return [this](std::string_view path, std::string* error) -> std::optional<Json> {
            auto it = text.find(std::string(path));
            if (it == text.end()) {
                if (error) *error = "no such file";
                return std::nullopt;
            }
            return Json::parse(it->second, error);
        };
    }
};

const char* kBase = R"({"format":"astral.material","version":1,"shader":"shaders/leaf.ashader",
    "values":{"tint":[1,0,0,1],"sway":0.2,"albedo":"textures/leaf.png"}})";

} // namespace

TEST_CASE("a material with a parent takes the shader and the values it does not override") {
    Files files;
    files.text["materials/base.material"] = kBase;
    files.text["materials/windy.material"] =
        R"({"format":"astral.material","version":1,"parent":"materials/base.material","values":{"sway":0.5}})";
    const MaterialFile m = resolve_material("materials/windy.material", files.reader());
    REQUIRE_MESSAGE(m.ok(), (m.errors.empty() ? "" : m.errors[0]));
    CHECK(m.shader == "shaders/leaf.ashader");
    CHECK(m.dependencies == std::vector<std::string>{"materials/windy.material", "materials/base.material"});
    const Json* sway = m.value("sway");
    REQUIRE(sway != nullptr);
    CHECK(sway->as_number() == doctest::Approx(0.5));
    REQUIRE(m.value("tint") != nullptr);
    CHECK((*m.value("tint"))[0].as_number() == doctest::Approx(1.0));
    CHECK(m.value("nothing") == nullptr);
}

TEST_CASE("a child can switch the shader") {
    Files files;
    files.text["a.material"] = kBase;
    files.text["b.material"] = R"({"format":"astral.material","version":1,"parent":"a.material","shader":"shaders/other.ashader"})";
    const MaterialFile m = resolve_material("b.material", files.reader());
    REQUIRE(m.ok());
    CHECK(m.shader == "shaders/other.ashader");
}

TEST_CASE("bad materials are reported, not crashed on") {
    Files files;
    files.text["a.material"] = R"({"format":"astral.material","version":1,"parent":"b.material"})";
    files.text["b.material"] = R"({"format":"astral.material","version":1,"parent":"a.material","shader":"x.ashader"})";
    SUBCASE("cycle") {
        const MaterialFile m = resolve_material("a.material", files.reader());
        CHECK_FALSE(m.ok());
        REQUIRE_FALSE(m.errors.empty());
        CHECK(m.errors[0].find("cycle") != std::string::npos);
    }
    SUBCASE("missing file and missing parent") {
        CHECK_FALSE(resolve_material("none.material", files.reader()).ok());
        files.text["c.material"] = R"({"format":"astral.material","version":1,"parent":"gone.material"})";
        CHECK_FALSE(resolve_material("c.material", files.reader()).ok());
    }
    SUBCASE("no shader anywhere in the chain") {
        files.text["d.material"] = R"({"format":"astral.material","version":1})";
        const MaterialFile m = resolve_material("d.material", files.reader());
        CHECK_FALSE(m.ok());
    }
    SUBCASE("wrong format") {
        files.text["e.material"] = R"({"format":"astral.scene","version":1})";
        CHECK_FALSE(resolve_material("e.material", files.reader()).ok());
    }
}

TEST_CASE("evaluate_uniforms turns values into the parameter block") {
    const ShaderParse p = parse_ashader(R"(shader_type surface;
uniform float a = 1.0;
uniform vec3 glow = vec3(0.0, 0.5, 0.0);
uniform sampler2D albedo : source_color;
uniform sampler2D bump : hint_normal;
uniform vec2 scroll = vec2(1.0, 2.0);
uniform int steps = 4;
uniform bool flip = false;
void surface() {}
)");
    REQUIRE(p.errors.empty());
    const UniformLayout layout = layout_uniforms(p.source);
    Files files;
    files.text["m.material"] = R"({"format":"astral.material","version":1,"shader":"s.ashader",
        "values":{"a":3.5,"glow":[1,0,0],"albedo":"tex/a.png","bump":"tex/n.png","flip":true,"bogus":1,"scroll":"oops"}})";
    const MaterialFile m = resolve_material("m.material", files.reader());
    REQUIRE(m.ok());
    const UniformValues v = evaluate_uniforms(p.source, layout, m);
    REQUIRE(v.params.size() == layout.words);
    auto f = [&](std::uint32_t i) { return std::bit_cast<float>(v.params[i]); }; // float words (ADR-0042)
    CHECK(f(layout.offsets[0]) == doctest::Approx(3.5f));
    CHECK(f(layout.offsets[1] + 0) == doctest::Approx(1.0f)); // overridden
    CHECK(f(layout.offsets[1] + 1) == doctest::Approx(0.0f));
    CHECK(f(layout.offsets[4] + 0) == doctest::Approx(1.0f)); // wrong type: default kept
    CHECK(f(layout.offsets[4] + 1) == doctest::Approx(2.0f));
    CHECK(v.params[layout.offsets[5]] == 4u); // int default, exact
    CHECK(v.params[layout.offsets[6]] == 1u); // bool true
    REQUIRE(v.textures.size() == 2);
    CHECK(v.textures[0].path == "tex/a.png");
    CHECK(v.textures[0].offset == layout.offsets[2]);
    CHECK(v.textures[0].srgb);
    CHECK_FALSE(v.textures[0].normal);
    CHECK(v.textures[1].path == "tex/n.png");
    CHECK(v.textures[1].normal);
    CHECK_FALSE(v.textures[1].srgb);
    CHECK(v.warnings.size() == 2); // "bogus" is no uniform, "scroll" has the wrong type
}

TEST_CASE("defaults fill what the material leaves out") {
    const ShaderParse p = parse_ashader("shader_type surface;\nuniform vec4 tint = vec4(0.1, 0.2, 0.3, 0.4);\nvoid surface() {}\n");
    REQUIRE(p.errors.empty());
    MaterialFile m;
    m.shader = "s.ashader";
    const UniformValues v = evaluate_uniforms(p.source, layout_uniforms(p.source), m);
    CHECK(std::bit_cast<float>(v.params[3]) == doctest::Approx(0.4f));
    CHECK(v.warnings.empty());
}

TEST_CASE("booleans are accepted as vector components like they are as scalars") {
    const ShaderParse p = parse_ashader("shader_type surface;\nuniform vec2 mask = vec2(0.0);\nvoid surface() {}\n");
    REQUIRE(p.errors.empty());
    MaterialFile m;
    m.shader = "s.ashader";
    m.values.emplace_back("mask", *Json::parse("[true,false]", nullptr));
    const UniformValues v = evaluate_uniforms(p.source, layout_uniforms(p.source), m);
    CHECK(v.warnings.empty());
    CHECK(std::bit_cast<float>(v.params[0]) == doctest::Approx(1.0f));
    CHECK(std::bit_cast<float>(v.params[1]) == doctest::Approx(0.0f));
}
