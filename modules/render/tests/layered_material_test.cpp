#include <astral/render/layered_material.hpp>
#include <astral/shader/ashader.hpp>

#include <doctest/doctest.h>

#include <map>

using namespace astral;

namespace {

struct Files {
    std::map<std::string, std::string> text;
    MaterialReader materials() const {
        return [this](std::string_view path, std::string* error) -> std::optional<Json> {
            auto it = text.find(std::string(path));
            if (it == text.end()) {
                if (error) *error = "no such file";
                return std::nullopt;
            }
            return Json::parse(it->second, error);
        };
    }
    TextReader texts() const {
        return [this](std::string_view path, std::string* error) -> std::optional<std::string> {
            auto it = text.find(std::string(path));
            if (it == text.end()) {
                if (error) *error = "no such file";
                return std::nullopt;
            }
            return it->second;
        };
    }
};

Files rock_and_moss() {
    Files f;
    f.text["rock.material"] = R"({"format":"astral.material","version":1,"type":"pbr",
        "values":{"base_color":[0.4,0.4,0.4,1],"roughness":0.9,"base_color_texture":"tex/rock.png","normal_texture":"tex/rock_n.png"}})";
    f.text["moss.material"] = R"({"format":"astral.material","version":1,"shader":"shaders/moss.ashader","values":{"tint":[0.1,0.6,0.1]}})";
    f.text["shaders/moss.ashader"] = R"(shader_type surface;
uniform vec3 tint : source_color = vec3(1.0);
uniform sampler2D detail : repeat = "tex/moss_detail.png";
float wobble(float x) { return sin(x * 3.0); }
void surface() { ALBEDO = tint * (0.8 + 0.2 * wobble(UV.x)) * texture(detail, UV).r; ROUGHNESS = 1.0; }
)";
    return f;
}

LayeredShader compose(const Files& f, const char* layers, std::string_view model = {}, std::vector<std::string> modes = {}) {
    return compose_layered_shader(*Json::parse(layers), model, modes, f.materials(), f.texts());
}

} // namespace

TEST_CASE("a PBR base and a shader layer compose into one parseable surface shader") {
    const Files f = rock_and_moss();
    const LayeredShader l = compose(f, R"([{"material":"rock.material"},
        {"material":"moss.material","mask":{"texture":"tex/mask.png","channel":"g","slope":[0.6,0.9],"height_world":[0,8],
         "noise":{"scale":2,"amount":0.4},"contrast":0.3,"height_blend":true,"depth":0.15},
         "height":{"texture":"tex/h.png","channel":"b"}}])", "clearcoat", {"cull_disabled"});
    REQUIRE_MESSAGE(l.ok(), (l.errors.empty() ? "" : l.errors[0]));
    const ShaderParse p = parse_ashader(l.source);
    REQUIRE_MESSAGE(p.errors.empty(), (p.errors.empty() ? "" : std::to_string(p.errors[0].line) + ": " + p.errors[0].message + "\n" + l.source));
    CHECK(p.source.type == ShaderType::Surface);
    CHECK(p.source.shading_model == ShadingModel::Clearcoat);
    CHECK(p.source.has_render_mode("cull_disabled"));
    CHECK(p.source.has_function("surface"));
    auto find = [&](const char* name) -> const ShaderUniform* {
        for (const ShaderUniform& u : p.source.uniforms) {
            if (u.name == name) return &u;
        }
        return nullptr;
    };
    // Layer uniforms are prefixed; the material's values and the shaders' defaults become the defaults.
    REQUIRE(find("layer0_roughness"));
    CHECK(find("layer0_roughness")->default_value == std::vector<double>{0.9});
    REQUIRE(find("layer0_base_color_texture"));
    CHECK(find("layer0_base_color_texture")->default_texture == "tex/rock.png");
    REQUIRE(find("layer1_tint"));
    CHECK(find("layer1_tint")->default_value == std::vector<double>{0.1, 0.6, 0.1}); // the .material overrides the shader default
    CHECK(find("layer1_tint")->hints.source_color);
    REQUIRE(find("layer1_detail"));
    CHECK(find("layer1_detail")->default_texture == "tex/moss_detail.png");
    REQUIRE(find("layer1_mask"));
    REQUIRE(find("layer1_mask_texture"));
    CHECK(find("layer1_mask_texture")->default_texture == "tex/mask.png");
    REQUIRE(find("layer1_depth"));
    CHECK(find("layer1_depth")->default_value == std::vector<double>{0.15});
    CHECK(l.dependencies == std::vector<std::string>{"rock.material", "moss.material", "shaders/moss.ashader"});
}

TEST_CASE("a layer shader's helper functions and uniforms cannot collide with another layer's") {
    Files f = rock_and_moss();
    f.text["moss2.material"] = R"({"format":"astral.material","version":1,"shader":"shaders/moss.ashader"})";
    const LayeredShader l = compose(f, R"([{"material":"moss.material"},{"material":"moss2.material","mask":{"value":0.5}}])");
    REQUIRE_MESSAGE(l.ok(), (l.errors.empty() ? "" : l.errors[0]));
    const ShaderParse p = parse_ashader(l.source);
    CHECK(p.errors.empty());
    CHECK(l.source.find("astral_l0_wobble") != std::string::npos);
    CHECK(l.source.find("astral_l1_wobble") != std::string::npos);
}

TEST_CASE("layered composition reports problems per layer instead of crashing") {
    Files f = rock_and_moss();
    f.text["vert.ashader"] = "shader_type surface;\nvoid vertex() {}\nvoid surface() {}\n";
    f.text["post.ashader"] = "shader_type post;\nvoid post() {}\n";
    f.text["layered.material"] = R"({"format":"astral.material","version":1,"type":"layered","layers":[{"material":"rock.material"}]})";
    SUBCASE("not an array / empty") {
        CHECK_FALSE(compose(f, "{}").ok());
        CHECK_FALSE(compose(f, "[]").ok());
    }
    SUBCASE("too many layers") {
        std::string many = "[";
        for (int i = 0; i < 9; ++i) many += std::string(i ? "," : "") + R"({"material":"rock.material"})";
        CHECK_FALSE(compose(f, (many + "]").c_str()).ok());
    }
    SUBCASE("missing material path or file") {
        const LayeredShader a = compose(f, R"([{"mask":{}}])");
        REQUIRE_FALSE(a.errors.empty());
        CHECK(a.errors[0].find("layer 0") != std::string::npos);
        CHECK_FALSE(compose(f, R"([{"material":"gone.material"}])").ok());
    }
    SUBCASE("a layer shader with vertex() or the wrong type") {
        const LayeredShader a = compose(f, R"([{"material":"vert.ashader"}])");
        REQUIRE_FALSE(a.errors.empty());
        CHECK(a.errors[0].find("vertex") != std::string::npos);
        CHECK_FALSE(compose(f, R"([{"material":"post.ashader"}])").ok());
    }
    SUBCASE("nested layered materials are refused") {
        const LayeredShader a = compose(f, R"([{"material":"layered.material"}])");
        REQUIRE_FALSE(a.errors.empty());
        CHECK(a.errors[0].find("layered") != std::string::npos);
    }
    SUBCASE("bad mask and height shapes") {
        CHECK_FALSE(compose(f, R"([{"material":"rock.material"},{"material":"rock.material","mask":{"slope":[1]}}])").ok());
        CHECK_FALSE(compose(f, R"([{"material":"rock.material"},{"material":"rock.material","mask":{"texture":"a.png","channel":"x"}}])").ok());
        CHECK_FALSE(compose(f, R"([{"material":"rock.material"},{"material":"rock.material","height":"tall"}])").ok());
        CHECK_FALSE(compose(f, R"([{"material":"rock.material"},{"material":"rock.material","mask":{"texture":"a\"b.png"}}])").ok());
    }
}
