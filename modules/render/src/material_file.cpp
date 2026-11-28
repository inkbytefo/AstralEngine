#include <astral/render/material_file.hpp>

#include <algorithm>

namespace astral {

namespace {

constexpr std::size_t max_depth = 16;

std::size_t component_count(UniformType t) {
    switch (t) {
    case UniformType::Vec2: return 2;
    case UniformType::Vec3: return 3;
    case UniformType::Vec4: return 4;
    default: return 1;
    }
}

/// Adds the members `from` has and `into` lacks, recursing into objects; `into` (the child) wins.
void fill_missing(Json& into, const Json& from) {
    for (const auto& [key, value] : from.as_object()) {
        const bool present = into.find(key) != nullptr;
        Json& slot = into[key];
        if (!present) slot = value;
        else if (slot.is_object() && value.is_object()) fill_missing(slot, value);
    }
}

} // namespace

const Json* MaterialFile::value(std::string_view name) const noexcept {
    for (const auto& [key, v] : values) {
        if (key == name) return &v;
    }
    return nullptr;
}

MaterialFile resolve_material(std::string_view path, const MaterialReader& read) {
    MaterialFile m;
    std::string current(path);
    for (std::size_t depth = 0;; ++depth) {
        if (depth >= max_depth) {
            m.errors.push_back("material chain deeper than " + std::to_string(max_depth) + " at " + current);
            return m;
        }
        if (std::find(m.dependencies.begin(), m.dependencies.end(), current) != m.dependencies.end()) {
            m.errors.push_back("parent cycle through " + current);
            return m;
        }
        m.dependencies.push_back(current);
        std::string error;
        const std::optional<Json> doc = read(current, &error);
        if (!doc) {
            m.errors.push_back(current + ": " + error);
            return m;
        }
        if ((*doc)["format"].as_string() != "astral.material" || (*doc)["version"].as_number() != 1.0) {
            m.errors.push_back(current + ": not an astral.material v1 document");
            return m;
        }
        const Json& type = (*doc)["type"];
        const Json& shader = (*doc)["shader"];
        if ((!type.is_null() && type.as_string() != "pbr") || (type.as_string() == "pbr" && !shader.is_null())) {
            m.errors.push_back(current + ": expected type pbr or a shader path");
            return m;
        }
        if (!m.standard_pbr && m.shader.empty()) {
            m.standard_pbr = type.as_string() == "pbr";
            if (shader.is_string()) m.shader = shader.as_string();
        }
        for (const auto& [name, v] : (*doc)["values"].as_object()) {
            auto existing = std::find_if(m.values.begin(), m.values.end(), [&](const auto& e) { return e.first == name; });
            if (existing == m.values.end()) m.values.emplace_back(name, v); // the child (read first) wins
            else if (existing->second.is_object() && v.is_object()) fill_missing(existing->second, v); // field by field (ADR-0055 D2)
        }
        const Json& parent = (*doc)["parent"];
        if (!parent.is_string() || parent.as_string().empty()) break;
        current = parent.as_string();
    }
    if (!m.standard_pbr && m.shader.empty()) m.errors.push_back(std::string(path) + ": no shader or PBR type in the material or its parents");
    return m;
}

UniformValues evaluate_uniforms(const ShaderSource& shader, const UniformLayout& layout, const MaterialFile& material) {
    UniformValues out;
    out.params.assign(layout.words, 0u);
    for (std::size_t i = 0; i < shader.uniforms.size(); ++i) {
        const ShaderUniform& u = shader.uniforms[i];
        const std::uint32_t offset = layout.offsets[i];
        const Json* v = material.value(u.name);
        if (u.type == UniformType::Sampler2D) {
            if (!v) continue;
            if (!v->is_string()) {
                out.warnings.push_back("'" + u.name + "': a texture takes a path (string)");
                continue;
            }
            if (!v->as_string().empty()) out.textures.push_back({offset, v->as_string(), u.hints.source_color, u.hints.hint_normal});
            continue;
        }
        const std::size_t n = component_count(u.type);
        // Words through the same check as the .ashader defaults (ADR-0042); defaults already passed it.
        std::vector<std::uint32_t> words(n, uniform_word(u.type, 0.0).value_or(0u));
        for (std::size_t k = 0; k < n && k < u.default_value.size(); ++k) words[k] = uniform_word(u.type, u.default_value[k]).value_or(0u);
        if (v) {
            std::vector<std::uint32_t> given;
            bool ok = false;
            auto word = [&](const Json& c) {
                const std::optional<double> x = c.is_bool() ? std::optional<double>(c.as_bool() ? 1.0 : 0.0)
                                                : c.is_number() ? std::optional<double>(c.as_number())
                                                                : std::nullopt;
                return x ? uniform_word(u.type, *x) : std::nullopt;
            };
            if (v->is_number() || v->is_bool()) {
                if (const auto w = word(*v)) {
                    given.assign(n, *w); // a single number fills every component
                    ok = true;
                }
            } else if (v->is_array() && v->size() == n) {
                ok = true;
                for (const Json& c : v->as_array()) {
                    const auto w = (c.is_number() || c.is_bool()) ? word(c) : std::nullopt;
                    ok = ok && w.has_value();
                    given.push_back(w.value_or(0u));
                }
            }
            if (ok) words = std::move(given);
            else out.warnings.push_back("'" + u.name + "': expected " + std::string(to_string(u.type)) + ", got something else");
        }
        std::copy(words.begin(), words.end(), out.params.begin() + offset);
    }
    for (const auto& [name, value] : material.values) {
        (void)value;
        const bool known = std::any_of(shader.uniforms.begin(), shader.uniforms.end(), [&](const ShaderUniform& u) { return u.name == name; });
        if (!known) out.warnings.push_back("'" + name + "' is not a uniform of the shader");
    }
    return out;
}

} // namespace astral
