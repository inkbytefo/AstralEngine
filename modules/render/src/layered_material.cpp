#include <astral/render/layered_material.hpp>

#include <astral/shader/ashader.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace astral {

namespace {

std::string number(double v) {
    char buf[40];
    if (v == std::floor(v) && std::fabs(v) < 1e9) std::snprintf(buf, sizeof buf, "%.1f", v);
    else std::snprintf(buf, sizeof buf, "%.9g", v);
    return buf;
}

bool safe_path(std::string_view s) { return s.find('"') == std::string_view::npos && s.find('\n') == std::string_view::npos && s.find('\\') == std::string_view::npos; }

/// A JSON value as GLSL default text for a uniform of `type`; nullopt if it does not fit.
std::optional<std::string> default_text(UniformType type, const Json& v) {
    auto scalar = [](const Json& c) -> std::optional<double> {
        if (c.is_bool()) return c.as_bool() ? 1.0 : 0.0;
        if (c.is_number()) return c.as_number();
        return std::nullopt;
    };
    switch (type) {
    case UniformType::Float: {
        const auto x = scalar(v);
        return x ? std::optional<std::string>(number(*x)) : std::nullopt;
    }
    case UniformType::Int: {
        const auto x = scalar(v);
        return x ? std::optional<std::string>(std::to_string(static_cast<long long>(*x))) : std::nullopt;
    }
    case UniformType::Bool: {
        const auto x = scalar(v);
        return x ? std::optional<std::string>(*x != 0.0 ? "true" : "false") : std::nullopt;
    }
    case UniformType::Vec2:
    case UniformType::Vec3:
    case UniformType::Vec4: {
        const std::size_t n = type == UniformType::Vec2 ? 2 : type == UniformType::Vec3 ? 3 : 4;
        std::string out = std::string("vec") + std::to_string(n) + "(";
        if (const auto x = scalar(v)) return out + number(*x) + ")";
        if (!v.is_array() || v.size() != n) return std::nullopt;
        for (std::size_t i = 0; i < n; ++i) {
            const auto x = scalar(v[i]);
            if (!x) return std::nullopt;
            out += (i ? ", " : "") + number(*x);
        }
        return out + ")";
    }
    case UniformType::Sampler2D: return std::nullopt;
    }
    return std::nullopt;
}

const char* glsl_type(UniformType t) {
    switch (t) {
    case UniformType::Float: return "float";
    case UniformType::Vec2: return "vec2";
    case UniformType::Vec3: return "vec3";
    case UniformType::Vec4: return "vec4";
    case UniformType::Int: return "int";
    case UniformType::Bool: return "bool";
    case UniformType::Sampler2D: return "sampler2D";
    }
    return "float";
}

std::string hint_text(const UniformHints& h) {
    std::string s;
    auto add = [&](const std::string& part) { s += (s.empty() ? " : " : ", ") + part; };
    if (h.source_color) add("source_color");
    if (h.hint_normal) add("hint_normal");
    if (h.range) add("range(" + number(h.range->min) + ", " + number(h.range->max) + ", " + number(h.range->step) + ")");
    switch (h.filter) {
    case TextureFilter::Linear: add("filter_linear"); break;
    case TextureFilter::LinearMipmap: break; // the default
    case TextureFilter::Nearest: add("filter_nearest"); break;
    case TextureFilter::NearestMipmap: add("filter_nearest_mipmap"); break;
    }
    if (!h.repeat) add("repeat_disable");
    return s;
}

std::string uniform_line(const ShaderUniform& u, const std::string& name, const std::string& default_value) {
    std::string line = std::string("uniform ") + glsl_type(u.type) + " " + name + hint_text(u.hints);
    if (!default_value.empty()) line += " = " + default_value;
    return line + ";\n";
}

const char* channel_of(const Json& v, std::string& error) {
    const std::string c = v.is_string() ? v.as_string() : "r";
    if (c == "r") return "r";
    if (c == "g") return "g";
    if (c == "b") return "b";
    if (c == "a") return "a";
    error = "channel must be r, g, b or a";
    return "r";
}

struct Composer {
    LayeredShader out;
    std::string uniforms, functions;
    const TextReader& read_text;

    void error(std::size_t layer, const std::string& message) { out.errors.push_back("layer " + std::to_string(layer) + ": " + message); }
    void depend(const std::string& path) {
        if (std::find(out.dependencies.begin(), out.dependencies.end(), path) == out.dependencies.end()) out.dependencies.push_back(path);
    }

    /// `name` -> the numeric/vector value the layer's material gives it, else `fallback`.
    std::string value_or(const MaterialFile& m, const char* name, UniformType type, const std::string& fallback) {
        if (const Json* v = m.value(name)) {
            if (auto text = default_text(type, *v)) return *text;
        }
        return fallback;
    }

    void pbr_layer(std::size_t i, const MaterialFile& m) {
        const std::string p = "layer" + std::to_string(i) + "_";
        auto num = [&](const char* name, double fallback) { return value_or(m, name, UniformType::Float, number(fallback)); };
        uniforms += "uniform vec4 " + p + "base_color : source_color = " + value_or(m, "base_color", UniformType::Vec4, "vec4(1.0)") + ";\n";
        uniforms += "uniform float " + p + "roughness = " + num("roughness", 0.6) + ";\n";
        uniforms += "uniform float " + p + "metallic = " + num("metallic", 0.0) + ";\n";
        uniforms += "uniform vec3 " + p + "emissive = " + value_or(m, "emissive", UniformType::Vec3, "vec3(0.0)") + ";\n";
        uniforms += "uniform float " + p + "normal_scale = " + num("normal_scale", 1.0) + ";\n";
        uniforms += "uniform float " + p + "occlusion_strength = " + num("occlusion_strength", 1.0) + ";\n";
        Json scale = Json::array({1.0, 1.0});
        if (const Json* mapping = m.value("mapping")) {
            if ((*mapping)["uv_scale"].is_array()) scale = (*mapping)["uv_scale"];
        }
        uniforms += "uniform vec2 " + p + "uv_scale = " + default_text(UniformType::Vec2, scale).value_or("vec2(1.0)") + ";\n";
        struct Slot { const char* key; const char* hint; };
        const Slot slots[] = {{"base_color_texture", " : source_color, repeat"}, {"metallic_roughness_texture", " : repeat"},
                              {"normal_texture", " : hint_normal, repeat"}, {"emissive_texture", " : source_color, repeat"},
                              {"occlusion_texture", " : repeat"}};
        for (const Slot& s : slots) {
            std::string path;
            if (const Json* v = m.value(s.key); v && v->is_string()) path = v->as_string();
            if (!safe_path(path)) { error(i, std::string(s.key) + ": unsupported characters in the path"); continue; }
            uniforms += "uniform sampler2D " + p + s.key + s.hint + (path.empty() ? "" : " = \"" + path + "\"") + ";\n";
        }
        functions += "AstralLayer astral_l" + std::to_string(i) + "_eval() {\n    AstralLayer l = astral_layer_default();\n    vec2 uv = UV * " + p + "uv_scale;\n";
        functions += "    l.albedo = " + p + "base_color.rgb * texture(" + p + "base_color_texture, uv).rgb;\n";
        functions += "    vec4 mr = texture(" + p + "metallic_roughness_texture, uv);\n";
        functions += "    l.roughness = " + p + "roughness * mr.g;\n    l.metallic = " + p + "metallic * mr.b;\n";
        functions += "    l.emission = " + p + "emissive * texture(" + p + "emissive_texture, uv).rgb;\n";
        functions += "    l.ao = mix(1.0, texture(" + p + "occlusion_texture, uv).r, clamp(" + p + "occlusion_strength, 0.0, 1.0));\n";
        functions += "    vec2 nxy = (texture(" + p + "normal_texture, uv).xy * 2.0 - 1.0) * " + p + "normal_scale;\n";
        functions += "    l.normal_map = vec3(nxy * 0.5 + 0.5, 1.0);\n    return l;\n}\n";
    }

    void shader_layer(std::size_t i, const MaterialFile& m) {
        const std::string p = "layer" + std::to_string(i) + "_";
        std::string text_error;
        depend(m.shader);
        const auto text = read_text(m.shader, &text_error);
        if (!text) return error(i, m.shader + ": " + text_error);
        const ShaderParse parsed = parse_ashader(*text);
        if (!parsed.errors.empty()) return error(i, m.shader + ":" + std::to_string(parsed.errors[0].line) + ": " + parsed.errors[0].message);
        const ShaderSource& src = parsed.source;
        if (src.type != ShaderType::Surface) return error(i, m.shader + " is not a surface shader");
        if (src.has_function("vertex")) return error(i, m.shader + ": layer shaders cannot define vertex()");
        if (!src.varyings.empty()) return error(i, m.shader + ": layer shaders cannot declare varyings");
        if (!src.has_function("surface")) return error(i, m.shader + ": no surface() function");
        const std::string fn = "astral_l" + std::to_string(i) + "_";
        std::string macros, undefs;
        for (const ShaderUniform& u : src.uniforms) {
            std::string def;
            if (u.type == UniformType::Sampler2D) {
                std::string path = u.default_texture;
                if (const Json* v = m.value(u.name); v && v->is_string()) path = v->as_string();
                if (!safe_path(path)) { error(i, u.name + ": unsupported characters in the path"); continue; }
                if (!path.empty()) def = "\"" + path + "\"";
            } else {
                if (const Json* v = m.value(u.name)) {
                    if (auto t = default_text(u.type, *v)) def = *t;
                }
                if (def.empty() && !u.default_value.empty()) {
                    // The shader's own default: spell it as a vector/scalar of its components.
                    Json comps = Json::array();
                    for (double d : u.default_value) comps.push_back(Json(d));
                    def = default_text(u.type, u.default_value.size() == 1 ? comps[0] : comps).value_or("");
                }
            }
            uniforms += uniform_line(u, p + u.name, def);
            macros += "#define " + u.name + " " + p + u.name + "\n";
            undefs += "#undef " + u.name + "\n";
        }
        for (const std::string& f : src.functions) {
            const std::string renamed = f == "surface" ? fn + "surface" : fn + f;
            macros += "#define " + f + " " + renamed + "\n";
            undefs += "#undef " + f + "\n";
        }
        functions += macros + src.body + "\n" + undefs;
        functions += "AstralLayer " + fn + "eval() {\n" + detail_reset() + "    " + fn + "surface();\n";
        functions += "    AstralLayer l = astral_layer_default();\n    l.albedo = ALBEDO; l.roughness = ROUGHNESS; l.metallic = METALLIC;\n";
        functions += "    l.emission = EMISSION; l.ao = AO; l.normal_map = NORMAL_MAP;\n    return l;\n}\n";
    }

    static std::string detail_reset() {
        return "    ALBEDO = vec3(1.0); ROUGHNESS = 0.6; METALLIC = 0.0; EMISSION = vec3(0.0); ALPHA = 1.0;\n"
               "    ALPHA_SCISSOR = 0.5; NORMAL_MAP = vec3(0.5, 0.5, 1.0); NORMAL_MAP_DEPTH = 1.0; AO = 1.0;\n";
    }

    /// Mask and height uniforms/functions of layer i (i >= 1).
    void mask(std::size_t i, const Json& layer) {
        const std::string p = "layer" + std::to_string(i) + "_";
        const std::string fn = "astral_l" + std::to_string(i) + "_";
        const Json& mk = layer["mask"];
        double value = 1.0;
        if (mk["value"].is_number()) value = mk["value"].as_number();
        uniforms += "uniform float " + p + "mask : range(0.0, 1.0, 0.01) = " + number(value) + ";\n";
        std::string body = "float " + fn + "mask() {\n    float m = " + p + "mask;\n";
        std::string channel_error;
        if (mk["texture"].is_string()) {
            const std::string path = mk["texture"].as_string();
            if (!safe_path(path)) error(i, "mask.texture: unsupported characters in the path");
            double s = mk["uv_scale"].is_number() ? mk["uv_scale"].as_number() : 1.0;
            uniforms += "uniform sampler2D " + p + "mask_texture : repeat = \"" + path + "\";\n";
            uniforms += "uniform float " + p + "mask_uv_scale = " + number(s) + ";\n";
            body += std::string("    m *= texture(") + p + "mask_texture, UV * " + p + "mask_uv_scale)." + channel_of(mk["channel"], channel_error) + ";\n";
        }
        auto pair = [&](const char* key, double& a, double& b) {
            const Json& v = mk[key];
            if (v.is_null()) return false;
            if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number()) {
                error(i, std::string("mask.") + key + " is [from, to]");
                return false;
            }
            a = v[0].as_number();
            b = v[1].as_number();
            return true;
        };
        double a = 0, b = 1;
        if (pair("slope", a, b)) body += "    m *= astral_mask_slope(NORMAL, " + number(a) + ", " + number(b) + ");\n";
        if (pair("height_world", a, b)) body += "    m *= astral_mask_height(WORLD_POSITION, " + number(a) + ", " + number(b) + ");\n";
        if (mk["noise"].is_object()) {
            const double scale = mk["noise"]["scale"].as_number(1.0), amount = std::clamp(mk["noise"]["amount"].as_number(1.0), 0.0, 1.0);
            body += "    m *= mix(" + number(1.0 - amount) + ", 1.0, astral_mask_noise(WORLD_POSITION * " + number(scale) + "));\n";
        }
        if (mk["invert"].as_bool(false)) body += "    m = 1.0 - m;\n";
        if (const double c = mk["contrast"].as_number(0.0); c > 0.0) body += "    m = astral_mask_contrast(m, " + number(std::clamp(c, 0.0, 0.999)) + ");\n";
        if (!channel_error.empty()) error(i, "mask: " + channel_error);
        functions += body + "    return clamp(m, 0.0, 1.0);\n}\n";
        // height used by height blending
        const Json& h = layer["height"];
        std::string height = "0.5";
        if (h.is_number()) height = number(h.as_number());
        else if (h.is_object() && h["texture"].is_string()) {
            const std::string path = h["texture"].as_string();
            if (!safe_path(path)) error(i, "height.texture: unsupported characters in the path");
            uniforms += "uniform sampler2D " + p + "height_texture : repeat = \"" + path + "\";\n";
            uniforms += "uniform float " + p + "height_scale = " + number(h["scale"].as_number(1.0)) + ";\n";
            height = std::string("texture(") + p + "height_texture, UV)." + channel_of(h["channel"], channel_error) + " * " + p + "height_scale";
        } else if (!h.is_null()) {
            error(i, "height is a number or {texture, channel, scale}");
        }
        uniforms += "uniform float " + p + "depth : range(0.001, 1.0, 0.001) = " + number(std::clamp(mk["depth"].as_number(0.2), 0.001, 1.0)) + ";\n";
        functions += "float " + fn + "height() { return " + height + "; }\n";
    }
};

} // namespace

LayeredShader compose_layered_shader(const Json& layers, std::string_view shading_model,
                                     const std::vector<std::string>& render_modes,
                                     const MaterialReader& read_material, const TextReader& read_text) {
    Composer c{.out = {}, .uniforms = {}, .functions = {}, .read_text = read_text};
    if (!layers.is_array() || layers.size() < 1) {
        c.out.errors.push_back("layers: expected an array with at least one layer");
        return c.out;
    }
    if (layers.size() > max_material_layers) {
        c.out.errors.push_back("layers: at most " + std::to_string(max_material_layers) + " layers");
        return c.out;
    }
    std::vector<bool> height_blend;
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const Json& layer = layers[i];
        const Json& path = layer["material"];
        if (!path.is_string() || path.as_string().empty()) {
            c.error(i, "expected \"material\": path to a .material (or an .ashader)");
            continue;
        }
        MaterialFile file;
        if (path.as_string().ends_with(".ashader")) {
            file.shader = path.as_string();
            file.dependencies.push_back(file.shader);
        } else {
            file = resolve_material(path.as_string(), read_material);
        }
        for (const std::string& d : file.dependencies) c.depend(d);
        if (!file.ok()) {
            for (const std::string& e : file.errors) c.error(i, e);
            continue;
        }
        if (file.layered) {
            c.error(i, path.as_string() + " is itself layered; flatten it into one layer list");
            continue;
        }
        if (file.standard_pbr) c.pbr_layer(i, file);
        else c.shader_layer(i, file);
        if (i > 0) c.mask(i, layer);
        else {
            c.functions += "float astral_l0_height() { return " +
                           (layer["height"].is_number() ? number(layer["height"].as_number()) : std::string("0.5")) + "; }\n";
        }
        height_blend.push_back(i > 0 && layer["mask"]["height_blend"].as_bool(false));
    }
    if (!c.out.errors.empty()) return c.out;

    std::string s = "// Generated by Astral from a layered material (ADR-0055 D7). Edit the .material, not this text.\nshader_type surface;\n";
    if (!shading_model.empty()) s += "shading_model " + std::string(shading_model) + ";\n";
    if (!render_modes.empty()) {
        s += "render_mode ";
        for (std::size_t i = 0; i < render_modes.size(); ++i) s += (i ? ", " : "") + render_modes[i];
        s += ";\n";
    }
    s += c.uniforms + c.functions;
    s += "void surface() {\n    AstralLayer acc = astral_l0_eval();\n    acc.height = astral_l0_height();\n";
    for (std::size_t i = 1; i < layers.size(); ++i) {
        const std::string n = std::to_string(i);
        s += "    {\n        AstralLayer l = astral_l" + n + "_eval();\n        l.height = astral_l" + n + "_height();\n";
        s += height_blend[i] ? "        acc = astral_layer_blend_height(acc, l, astral_l" + n + "_mask(), layer" + n + "_depth);\n"
                             : "        acc = astral_layer_mix(acc, l, astral_l" + n + "_mask());\n";
        s += "    }\n";
    }
    s += "    ALBEDO = acc.albedo; ROUGHNESS = acc.roughness; METALLIC = acc.metallic; EMISSION = acc.emission;\n";
    s += "    AO = acc.ao; NORMAL_MAP = acc.normal_map;\n}\n";
    c.out.source = std::move(s);
    return c.out;
}

} // namespace astral
