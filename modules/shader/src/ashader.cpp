#include <astral/shader/ashader.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>

namespace astral {

namespace {

bool is_ident(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

/// Replaces comments with spaces (newlines kept): same length, same lines.
std::string strip_comments(std::string_view text) {
    std::string out(text);
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '/') {
            while (i < out.size() && out[i] != '\n') out[i++] = ' ';
        } else if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '*') {
            out[i] = out[i + 1] = ' ';
            i += 2;
            while (i < out.size() && !(out[i] == '*' && i + 1 < out.size() && out[i + 1] == '/')) {
                if (out[i] != '\n') out[i] = ' ';
                ++i;
            }
            if (i < out.size()) out[i] = ' ';
            if (i + 1 < out.size()) out[i + 1] = ' ';
            ++i;
        }
    }
    return out;
}

/// Splits on commas that are not inside parentheses.
std::vector<std::string_view> split_top(std::string_view s, char sep) {
    std::vector<std::string_view> parts;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') --depth;
        else if (s[i] == sep && depth == 0) {
            parts.push_back(trim(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    parts.push_back(trim(s.substr(start)));
    return parts;
}

std::optional<float> parse_number(std::string_view s) {
    s = trim(s);
    if (!s.empty() && (s.back() == 'f' || s.back() == 'F' || s.back() == 'u' || s.back() == 'U')) s.remove_suffix(1);
    if (s == "true") return 1.0f;
    if (s == "false") return 0.0f;
    if (s.empty()) return std::nullopt;
    const std::string copy(s);
    char* end = nullptr;
    const float v = std::strtof(copy.c_str(), &end);
    if (end != copy.c_str() + copy.size()) return std::nullopt;
    return v;
}

std::optional<UniformType> uniform_type(std::string_view t) {
    if (t == "float") return UniformType::Float;
    if (t == "vec2") return UniformType::Vec2;
    if (t == "vec3") return UniformType::Vec3;
    if (t == "vec4") return UniformType::Vec4;
    if (t == "int") return UniformType::Int;
    if (t == "bool") return UniformType::Bool;
    if (t == "sampler2D") return UniformType::Sampler2D;
    return std::nullopt;
}

std::size_t components(UniformType t) {
    switch (t) {
    case UniformType::Vec2: return 2;
    case UniformType::Vec3: return 3;
    case UniformType::Vec4: return 4;
    case UniformType::Sampler2D: return 0;
    default: return 1;
    }
}

/// "vec4(1.0)", "vec3(1, 0, 0.5)", "0.3", "true" → components.
std::optional<std::vector<float>> parse_default(std::string_view s, UniformType type) {
    s = trim(s);
    const std::size_t n = components(type);
    if (n == 0) return std::nullopt;
    std::vector<float> values;
    const std::size_t open = s.find('(');
    if (open != std::string_view::npos) {
        if (s.back() != ')') return std::nullopt;
        for (std::string_view part : split_top(s.substr(open + 1, s.size() - open - 2), ',')) {
            auto v = parse_number(part);
            if (!v) return std::nullopt;
            values.push_back(*v);
        }
    } else {
        auto v = parse_number(s);
        if (!v) return std::nullopt;
        values.push_back(*v);
    }
    if (values.size() == 1 && n > 1) {
        const auto first = values[0]; // copy: assign() must not read from the vector it overwrites
        values.assign(n, first);
    }
    if (values.size() != n) return std::nullopt;
    return values;
}

bool reserved(std::string_view name) {
    if (name.starts_with("astral_") || name.starts_with("gl_")) return true;
    return std::all_of(name.begin(), name.end(), [](char c) { return std::isupper(static_cast<unsigned char>(c)) || c == '_' || std::isdigit(static_cast<unsigned char>(c)); });
}

std::optional<ShaderType> shader_type(std::string_view t) {
    if (t == "surface") return ShaderType::Surface;
    if (t == "particle") return ShaderType::Particle;
    if (t == "volume") return ShaderType::Volume;
    if (t == "post") return ShaderType::Post;
    if (t == "sky") return ShaderType::Sky;
    if (t == "ui") return ShaderType::Ui;
    return std::nullopt;
}

struct Parser {
    std::string clean;
    ShaderParse result;
    bool has_type = false;

    std::uint32_t line_at(std::size_t pos) const {
        return static_cast<std::uint32_t>(1 + std::count(clean.begin(), clean.begin() + static_cast<std::ptrdiff_t>(pos), '\n'));
    }
    void error(std::size_t pos, std::string message) { result.errors.push_back({line_at(pos), std::move(message)}); }

    void uniform(std::string_view stmt, std::size_t pos) {
        ShaderUniform u;
        u.line = line_at(pos);
        std::string_view rest = trim(stmt.substr(std::string_view("uniform").size()));
        std::string_view def;
        if (auto parts = split_top(rest, '='); parts.size() == 2) {
            rest = parts[0];
            def = parts[1];
        } else if (parts.size() > 2) {
            return error(pos, "uniform: more than one '='");
        }
        std::string_view hints;
        if (const std::size_t colon = rest.find(':'); colon != std::string_view::npos) {
            hints = trim(rest.substr(colon + 1));
            rest = trim(rest.substr(0, colon));
        }
        const std::size_t space = rest.find_first_of(" \t\r\n");
        if (space == std::string_view::npos) return error(pos, "uniform: expected '<type> <name>'");
        const std::string_view type_name = trim(rest.substr(0, space));
        const std::string_view name = trim(rest.substr(space));
        const auto type = uniform_type(type_name);
        if (!type) return error(pos, "uniform '" + std::string(name) + "': unsupported type '" + std::string(type_name) + "'");
        if (name.empty() || !std::all_of(name.begin(), name.end(), is_ident)) return error(pos, "uniform: bad name '" + std::string(name) + "'");
        if (reserved(name)) return error(pos, "uniform '" + std::string(name) + "': reserved name (built-ins are UPPER_CASE, astral_ and gl_ are the engine's)");
        for (const ShaderUniform& other : result.source.uniforms) {
            if (other.name == name) return error(pos, "uniform '" + std::string(name) + "' declared twice");
        }
        u.name = name;
        u.type = *type;
        if (!hints.empty()) {
            for (std::string_view h : split_top(hints, ',')) {
                if (h == "source_color") u.hints.source_color = true;
                else if (h == "hint_normal") u.hints.hint_normal = true;
                else if (h == "filter_linear") u.hints.filter = TextureFilter::Linear;
                else if (h == "filter_linear_mipmap") u.hints.filter = TextureFilter::LinearMipmap;
                else if (h == "filter_nearest") u.hints.filter = TextureFilter::Nearest;
                else if (h == "filter_nearest_mipmap") u.hints.filter = TextureFilter::NearestMipmap;
                else if (h == "repeat") u.hints.repeat = true;
                else if (h == "repeat_disable") u.hints.repeat = false;
                else if (h.starts_with("range") || h.starts_with("hint_range")) {
                    const std::size_t open = h.find('(');
                    if (open == std::string_view::npos || h.back() != ')') return error(pos, "uniform '" + u.name + "': range(min, max[, step])");
                    const auto args = split_top(h.substr(open + 1, h.size() - open - 2), ',');
                    UniformRange r;
                    auto lo = parse_number(args[0]);
                    auto hi = args.size() > 1 ? parse_number(args[1]) : std::nullopt;
                    auto st = args.size() > 2 ? parse_number(args[2]) : std::optional<float>(0.0f);
                    if (!lo || !hi || !st || args.size() > 3) return error(pos, "uniform '" + u.name + "': range(min, max[, step])");
                    r.min = *lo;
                    r.max = *hi;
                    r.step = *st;
                    u.hints.range = r;
                } else {
                    return error(pos, "uniform '" + u.name + "': unknown hint '" + std::string(h) + "'");
                }
            }
        }
        if (!def.empty()) {
            auto value = parse_default(def, u.type);
            if (!value) return error(pos, "uniform '" + u.name + "': cannot read default '" + std::string(def) + "'");
            u.default_value = std::move(*value);
        }
        result.source.uniforms.push_back(std::move(u));
    }

    void statement(std::string_view stmt, std::size_t pos, std::size_t end) {
        std::size_t word_end = 0;
        while (word_end < stmt.size() && is_ident(stmt[word_end])) ++word_end;
        const std::string_view word = stmt.substr(0, word_end);
        bool header = true;
        if (word == "shader_type") {
            const std::string_view t = trim(stmt.substr(word_end));
            if (auto type = shader_type(t)) {
                result.source.type = *type;
                has_type = true;
            } else {
                error(pos, "unknown shader_type '" + std::string(t) + "' (surface, particle, volume, post, sky, ui)");
                has_type = true; // reported once
            }
        } else if (word == "render_mode") {
            for (std::string_view m : split_top(stmt.substr(word_end), ',')) {
                const auto known = known_render_modes(result.source.type);
                if (std::find(known.begin(), known.end(), m) == known.end()) error(pos, "unknown render_mode '" + std::string(m) + "'");
                else result.source.render_modes.emplace_back(m);
            }
        } else if (word == "uniform") {
            uniform(stmt, pos);
        } else if (word == "varying") {
            const std::string_view rest = trim(stmt.substr(word_end));
            const std::size_t space = rest.find_first_of(" \t\r\n");
            if (space == std::string_view::npos) error(pos, "varying: expected '<type> <name>'");
            else result.source.varyings.push_back({std::string(trim(rest.substr(0, space))), std::string(trim(rest.substr(space)))});
        } else {
            header = false;
        }
        if (header) {
            for (std::size_t i = pos; i <= end && i < result.source.body.size(); ++i) {
                if (result.source.body[i] != '\n') result.source.body[i] = ' ';
            }
        }
    }

    void run(std::string_view text) {
        clean = strip_comments(text);
        result.source.body = clean;
        std::size_t i = 0;
        while (i < clean.size()) {
            while (i < clean.size() && std::isspace(static_cast<unsigned char>(clean[i]))) ++i;
            if (i >= clean.size()) break;
            const std::size_t start = i;
            int parens = 0;
            while (i < clean.size()) {
                const char c = clean[i];
                if (c == '(') ++parens;
                else if (c == ')') --parens;
                else if (parens == 0 && (c == ';' || c == '{')) break;
                ++i;
            }
            if (i >= clean.size()) {
                error(start, "unexpected end of file (missing ';' or '{')");
                break;
            }
            if (clean[i] == ';') {
                statement(trim(std::string_view(clean).substr(start, i - start)), start, i);
                ++i;
                continue;
            }
            // A block: function (or struct). Name = last identifier before '('.
            const std::string_view head = std::string_view(clean).substr(start, i - start);
            bool function = false;
            if (const std::size_t open = head.find('('); open != std::string_view::npos) {
                std::size_t e = open;
                while (e > 0 && std::isspace(static_cast<unsigned char>(head[e - 1]))) --e;
                std::size_t b = e;
                while (b > 0 && is_ident(head[b - 1])) --b;
                if (b < e) {
                    result.source.functions.emplace_back(head.substr(b, e - b));
                    function = true;
                }
            }
            int depth = 0;
            const std::size_t block = i;
            for (; i < clean.size(); ++i) {
                if (clean[i] == '{') ++depth;
                else if (clean[i] == '}' && --depth == 0) break;
            }
            if (i >= clean.size()) {
                error(block, "unbalanced '{': block never closed");
                if (function) result.source.functions.pop_back();
                break;
            }
            ++i;
            if (function) result.source.function_ranges.emplace_back(start, i);
            while (i < clean.size() && (clean[i] == ';' || std::isspace(static_cast<unsigned char>(clean[i])))) ++i; // struct ... };
        }
        if (!has_type) result.errors.insert(result.errors.begin(), ShaderError{1, "missing 'shader_type' (e.g. shader_type surface;)"});
    }
};

} // namespace

bool ShaderSource::has_render_mode(std::string_view mode) const noexcept {
    return std::find(render_modes.begin(), render_modes.end(), mode) != render_modes.end();
}

std::string ShaderSource::body_without(std::string_view function) const {
    std::string out = body;
    for (std::size_t f = 0; f < functions.size() && f < function_ranges.size(); ++f) {
        if (functions[f] != function) continue;
        for (std::size_t i = function_ranges[f].first; i < function_ranges[f].second && i < out.size(); ++i) {
            if (out[i] != '\n') out[i] = ' ';
        }
    }
    return out;
}

bool ShaderSource::has_function(std::string_view name) const noexcept {
    return std::find(functions.begin(), functions.end(), name) != functions.end();
}

ShaderParse parse_ashader(std::string_view text) {
    Parser p;
    p.run(text);
    return std::move(p.result);
}

std::vector<std::string_view> known_render_modes(ShaderType type) {
    switch (type) {
    case ShaderType::Surface:
        return {"alpha_test", "cull_disabled", "cull_front", "unshaded", "blend_mix", "blend_add", "shadows_disabled", "rt_simple"};
    case ShaderType::Particle: return {"blend_mix", "blend_add", "unshaded"};
    case ShaderType::Volume: return {"unshaded"};
    case ShaderType::Post: return {"after_tonemap"};
    case ShaderType::Sky: return {};
    case ShaderType::Ui: return {"blend_mix", "blend_add"};
    }
    return {};
}

std::string_view to_string(ShaderType type) noexcept {
    switch (type) {
    case ShaderType::Surface: return "surface";
    case ShaderType::Particle: return "particle";
    case ShaderType::Volume: return "volume";
    case ShaderType::Post: return "post";
    case ShaderType::Sky: return "sky";
    case ShaderType::Ui: return "ui";
    }
    return "?";
}

std::string_view to_string(UniformType type) noexcept {
    switch (type) {
    case UniformType::Float: return "float";
    case UniformType::Vec2: return "vec2";
    case UniformType::Vec3: return "vec3";
    case UniformType::Vec4: return "vec4";
    case UniformType::Int: return "int";
    case UniformType::Bool: return "bool";
    case UniformType::Sampler2D: return "sampler2D";
    }
    return "?";
}

UniformLayout layout_uniforms(const ShaderSource& source) {
    UniformLayout layout;
    std::uint32_t at = 0;
    for (const ShaderUniform& u : source.uniforms) {
        std::uint32_t size = 1, align = 1;
        switch (u.type) {
        case UniformType::Vec2: size = 2; align = 2; break;
        case UniformType::Vec3: size = 3; align = 4; break;
        case UniformType::Vec4: size = 4; align = 4; break;
        default: break;
        }
        at = (at + align - 1) / align * align;
        layout.offsets.push_back(at);
        at += size;
    }
    layout.words = (at + 3) / 4 * 4;
    return layout;
}

} // namespace astral
