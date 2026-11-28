#include "material_library.hpp"
#include "json_asset.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/assets/model.hpp>
#include <astral/core/log.hpp>
#include <astral/render/layered_material.hpp>
#include <astral/world/serialize.hpp>

#include <algorithm>
#include <array>
#include <system_error>

namespace astral {

MaterialLibrary::MaterialLibrary(Renderer& renderer, AssetManager& assets, std::filesystem::path root, bool watch, Logger& log)
    : renderer_(renderer), assets_(assets), root_(std::move(root)), watch_(watch), log_(log) {}

MaterialLibrary::~MaterialLibrary() {
    for (auto& m : materials_) renderer_.destroy(m->handle);
    for (auto& s : shaders_) renderer_.destroy(s->handle);
    for (ShaderHandle h : retired_shaders_) renderer_.destroy(h);
    for (auto& [key, tex] : textures_) renderer_.destroy(tex.handle);
}

void MaterialLibrary::log_error(const std::string& what, const std::string& message) {
    log_.log(LogLevel::Error, what + ": " + message);
}

std::filesystem::file_time_type MaterialLibrary::time_of(const std::string& path) const {
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(root_ / path, ec);
    return ec ? std::filesystem::file_time_type{} : t;
}

std::optional<Json> MaterialLibrary::read_document(std::string_view path, std::string* error) {
    auto bytes = assets_.read(path, error);
    if (!bytes) return std::nullopt;
    return parse_json_asset(*bytes, error);
}

bool MaterialLibrary::reload_texture(Texture& tex) {
    std::string error;
    if (auto bytes = assets_.read(tex.path, &error)) {
        if (auto image = decode_image(*bytes, tex.srgb, &error)) {
            if (tex.handle.valid()) {
                if (renderer_.update_texture(tex.handle, *image)) return true;
            } else {
                tex.handle = renderer_.create_texture(*image);
                if (tex.handle.valid()) return true;
            }
        }
    }
    log_error("texture " + tex.path, error.empty() ? "cannot upload texture" : error);
    return false;
}

TextureHandle MaterialLibrary::texture(const std::string& path, bool srgb) {
    const std::string key = path + (srgb ? "#srgb" : "#linear");
    auto [entry, inserted] = textures_.try_emplace(key, Texture{path, srgb, {}, time_of(path)});
    if (inserted) reload_texture(entry->second);
    return entry->second.handle;
}

void MaterialLibrary::update_textures() {
    bool recovered = false;
    for (auto& [key, tex] : textures_) {
        const auto time = time_of(tex.path);
        if (tex.time == time) continue;
        tex.time = time;
        const bool missing = !tex.handle.valid();
        if (reload_texture(tex) && missing) recovered = true;
    }
    if (!recovered) return;
    ++revision_;

    for (auto& material : materials_) {
        if (material->file.standard_pbr) {
            std::vector<std::string> errors;
            MaterialDesc updated;
            if (build_pbr(material->file, updated, errors) && renderer_.update_material(material->handle, updated)) {
                material->desc = std::move(updated);
            }
            continue;
        }
        bool changed = false;
        for (std::size_t i = 0; i < material->texture_sources.size(); ++i) {
            const auto& source = material->texture_sources[i];
            auto& bound = material->desc.textures[i].texture;
            const TextureHandle handle = texture(source.path, source.srgb);
            if (bound == handle) continue;
            bound = handle;
            changed = true;
        }
        if (changed) renderer_.update_material(material->handle, material->desc);
    }
}

void MaterialLibrary::retire_shader(ShaderHandle old_handle) {
    if (!old_handle.valid()) return;
    const bool still_used = std::any_of(materials_.begin(), materials_.end(),
                                        [&](const auto& m) { return m->desc.shader == old_handle; });
    if (!still_used) renderer_.destroy(old_handle);
    else retired_shaders_.push_back(old_handle);
}

void MaterialLibrary::sweep_retired_shaders() {
    std::erase_if(retired_shaders_, [&](ShaderHandle h) {
        const bool used = std::any_of(materials_.begin(), materials_.end(),
                                      [&](const auto& m) { return m->desc.shader == h; });
        if (!used) {
            renderer_.destroy(h);
            return true;
        }
        return false;
    });
}

/// Compiles a .ashader. With `reload`, an existing entry gets its new pipelines only if the compile
/// succeeds; any replaced handle is returned in `retired_old` so materials can be rebound before destroying it.
MaterialLibrary::Shader* MaterialLibrary::load_shader(const std::string& path, bool reload, ShaderHandle* retired_old) {
    Shader* existing = nullptr;
    if (const auto it = shader_index_.find(path); it != shader_index_.end()) existing = shaders_[it->second].get();
    if (existing && !reload) return existing;

    auto fail = [&](const std::string& message) -> Shader* {
        log_error("shader " + path, message);
        return existing;
    };
    std::string error;
    auto bytes = assets_.read(path, &error);
    if (!bytes) return fail(error);
    const ShaderParse parsed = parse_ashader(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (!parsed.errors.empty()) {
        std::string all;
        for (const ShaderError& e : parsed.errors) all += "\n  " + path + ":" + std::to_string(e.line) + ": " + e.message;
        return fail(all);
    }
    return install_shader(path, parsed, existing, retired_old);
}

/// Compiles an already parsed shader and registers it under `path` (a file, or a layered material's generated shader).
MaterialLibrary::Shader* MaterialLibrary::install_shader(const std::string& path, const ShaderParse& parsed, Shader* existing,
                                                         ShaderHandle* retired_old) {
    auto fail = [&](const std::string& message) -> Shader* {
        log_error("shader " + path, message);
        return existing;
    };
    ShaderHandle handle;
    UniformLayout layout;
    if (parsed.source.type == ShaderType::Surface) {
        const SurfaceShaderBuild build = compile_surface_shader(parsed.source, path, renderer_.shader_compile_services());
        if (!build.ok()) {
            std::string all;
            for (const ShaderError& e : build.errors) all += "\n  " + path + ":" + std::to_string(e.line) + ": " + e.message;
            return fail(all);
        }
        handle = renderer_.create_shader(build, path);
        layout = build.layout;
    } else if (parsed.source.type == ShaderType::Post || parsed.source.type == ShaderType::Sky ||
               parsed.source.type == ShaderType::Volume || parsed.source.type == ShaderType::Particle) {
        const ScreenShaderBuild build = compile_screen_shader(parsed.source, path, renderer_.shader_compile_services());
        if (!build.ok()) {
            std::string all;
            for (const ShaderError& e : build.errors) all += "\n  " + path + ":" + std::to_string(e.line) + ": " + e.message;
            return fail(all);
        }
        handle = renderer_.create_shader(build, path);
        layout = build.layout;
    } else {
        return fail(std::string("shader_type ") + std::string(to_string(parsed.source.type)) + " is not supported yet");
    }
    if (!handle.valid()) return fail("the GPU rejected the pipelines");
    if (existing) {
        if (retired_old) *retired_old = existing->handle;
        existing->handle = handle;
        existing->source = parsed.source;
        existing->layout = layout;
        existing->time = time_of(path);
        return existing;
    }
    auto entry = std::make_unique<Shader>();
    entry->path = path;
    entry->handle = handle;
    entry->source = parsed.source;
    entry->layout = layout;
    entry->time = time_of(path);
    shader_index_.emplace(entry->path, shaders_.size());
    shaders_.push_back(std::move(entry));
    return shaders_.back().get();
}

namespace {
struct PbrTextureSlot {
    std::string_view name;
    TextureHandle& handle;
    bool srgb;
};
/// The standard PBR texture parameters of `desc` (names as in .material files).
std::array<PbrTextureSlot, 8> pbr_texture_slots(MaterialDesc& desc) {
    return {{{"base_color_texture", desc.base_color_texture, true},
             {"metallic_roughness_texture", desc.metallic_roughness_texture, false},
             {"normal_texture", desc.normal_texture, false},
             {"emissive_texture", desc.emissive_texture, true},
             {"occlusion_texture", desc.occlusion_texture, false},
             {"detail_albedo_texture", desc.detail.albedo_texture, true},
             {"detail_normal_texture", desc.detail.normal_texture, false},
             {"detail_height_texture", desc.detail.height_texture, false}}};
}
} // namespace

bool MaterialLibrary::build_pbr(const MaterialFile& file, MaterialDesc& desc, std::vector<std::string>& errors) {
    auto slots = pbr_texture_slots(desc);
    Json factors = Json::object();
    for (const auto& [name, value] : file.values) {
        const auto slot = std::find_if(slots.begin(), slots.end(), [&](const auto& s) { return s.name == name; });
        if (slot != slots.end()) {
            if (!value.is_string()) errors.push_back(name + ": expected an asset path");
            else if (!value.as_string().empty()) slot->handle = texture(value.as_string(), slot->srgb);
            continue;
        }
        bool known = false;
        for_each_field<MaterialDesc>([&](auto field) { known |= field.name == name; });
        if (!known) log_.log(LogLevel::Warn, "material " + file.dependencies.front() + ": '" + name + "' is not a PBR parameter"); // a typo must not kill a running scene on hot reload
        else factors[name] = value;
    }
    const bool parsed = from_json(factors, desc, {.resolve = {}, .errors = &errors, .path = "values"});
    return parsed && errors.empty();
}

/// (Re)builds a material from its files. On failure nothing changes (false).
bool MaterialLibrary::build(Material& material, bool create) {
    const MaterialReader reader = [this](std::string_view path, std::string* error) { return read_document(path, error); };
    MaterialFile file;
    if (material.path.ends_with(".ashader")) { // a shader on its own: every value is its default (ADR-0055 D3)
        file.shader = material.path;
        file.dependencies.push_back(material.path);
    } else {
        file = resolve_material(material.path, reader);
    }
    if (!file.ok()) {
        for (const std::string& e : file.errors) log_error("material " + material.path, e);
        return false;
    }
    const Shader* shader = nullptr;
    UniformValues values;
    MaterialDesc desc;
    ShaderHandle retired_old{};
    if (file.layered) { // one generated shader per layered material (ADR-0055 D7)
        const TextReader text_reader = [this](std::string_view path, std::string* error) -> std::optional<std::string> {
            auto bytes = assets_.read(path, error);
            if (!bytes) return std::nullopt;
            return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
        };
        const LayeredShader generated = compose_layered_shader(file.layers, file.shading_model, file.render_modes, reader, text_reader);
        for (const std::string& dep : generated.dependencies) {
            if (std::find(file.dependencies.begin(), file.dependencies.end(), dep) == file.dependencies.end()) file.dependencies.push_back(dep);
        }
        if (!generated.ok()) {
            for (const std::string& e : generated.errors) log_error("material " + material.path, e);
            return false;
        }
        const ShaderParse parsed = parse_ashader(generated.source);
        if (!parsed.errors.empty()) {
            for (const ShaderError& e : parsed.errors) log_error("material " + material.path, "generated shader:" + std::to_string(e.line) + ": " + e.message);
            return false;
        }
        const std::string key = material.path + "#layered";
        Shader* existing = nullptr;
        if (const auto it = shader_index_.find(key); it != shader_index_.end()) existing = shaders_[it->second].get();
        Shader* installed = install_shader(key, parsed, existing, &retired_old);
        if (!installed) return false;
        shader = installed;
        values = evaluate_uniforms(shader->source, shader->layout, file);
        for (const auto& warning : values.warnings) log_.log(LogLevel::Warn, "material " + material.path + ": " + warning);
        desc.shader = shader->handle;
        desc.params = values.params;
        for (const auto& source : values.textures) {
            desc.textures.push_back({source.offset, texture(source.path, source.srgb), source.normal});
        }
    } else if (file.standard_pbr) {
        std::vector<std::string> errors;
        if (!build_pbr(file, desc, errors)) {
            for (const auto& error : errors) log_error("material " + material.path, error);
            return false;
        }
    } else {
        shader = load_shader(file.shader, false);
        if (!shader) return false;
        values = evaluate_uniforms(shader->source, shader->layout, file);
        for (const auto& warning : values.warnings) log_.log(LogLevel::Warn, "material " + material.path + ": " + warning);
        desc.shader = shader->handle;
        desc.params = values.params;
        for (const auto& source : values.textures) {
            desc.textures.push_back({source.offset, texture(source.path, source.srgb), source.normal});
        }
    }
    if (create) material.handle = renderer_.create_material(desc);
    else if (!renderer_.update_material(material.handle, desc)) return false;
    if (!material.handle.valid()) return false;
    material.file = std::move(file);
    material.shader = shader;
    material.desc = std::move(desc);
    material.values = material.desc.params;
    material.texture_sources = values.textures;
    material.watched.clear();
    for (const std::string& dep : material.file.dependencies) material.watched.emplace_back(dep, time_of(dep));
    ++revision_;
    retire_shader(retired_old); // a regenerated layered shader: the material is rebound, the old pipelines can go
    return true;
}

MaterialHandle MaterialLibrary::derive(MaterialHandle base, std::span<const MaterialParam> params, MaterialHandle derived) {
    const Material* lib = nullptr;
    for (const auto& m : materials_) {
        if (m->handle == base) lib = m.get();
    }
    MaterialDesc desc;
    if (lib) {
        desc = lib->desc;
    } else if (const MaterialDesc* d = renderer_.material(base)) {
        desc = *d;
    } else {
        return derived;
    }
    const std::string label = "material override (" + (lib ? lib->path : std::string("handle")) + ")";
    const auto warn = [&](const std::string& message) { log_.log(LogLevel::Warn, label + ": " + message); };
    if (desc.shader.valid() && lib && lib->shader) {
        const ShaderSource& source = lib->shader->source;
        const UniformLayout& layout = lib->shader->layout;
        for (const MaterialParam& p : params) {
            const auto it = std::find_if(source.uniforms.begin(), source.uniforms.end(), [&](const ShaderUniform& u) { return u.name == p.name; });
            if (it == source.uniforms.end()) {
                warn("'" + p.name + "' is not a uniform of the shader");
                continue;
            }
            const ShaderUniform& u = *it;
            const std::uint32_t offset = layout.offsets[static_cast<std::size_t>(it - source.uniforms.begin())];
            if (u.type == UniformType::Sampler2D) {
                const TextureHandle tex = p.texture.empty() ? TextureHandle{} : texture(p.texture, u.hints.source_color);
                const auto bound = std::find_if(desc.textures.begin(), desc.textures.end(), [&](const auto& t) { return t.offset == offset; });
                if (bound != desc.textures.end()) bound->texture = tex;
                else desc.textures.push_back({offset, tex, u.hints.hint_normal});
                continue;
            }
            const std::size_t n = u.type == UniformType::Vec2 ? 2 : u.type == UniformType::Vec3 ? 3 : u.type == UniformType::Vec4 ? 4 : 1;
            const float comps[4] = {p.value.x, p.value.y, p.value.z, p.value.w};
            std::vector<std::uint32_t> words;
            for (std::size_t k = 0; k < n; ++k) {
                if (const auto w = uniform_word(u.type, comps[k])) words.push_back(*w);
            }
            if (words.size() != n || offset + n > desc.params.size()) {
                warn("'" + p.name + "': the value does not fit a " + std::string(to_string(u.type)));
                continue;
            }
            std::copy(words.begin(), words.end(), desc.params.begin() + offset);
        }
    } else {
        auto slots = pbr_texture_slots(desc);
        for (const MaterialParam& p : params) {
            if (const auto slot = std::find_if(slots.begin(), slots.end(), [&](const auto& s) { return s.name == p.name; }); slot != slots.end()) {
                slot->handle = p.texture.empty() ? TextureHandle{} : texture(p.texture, slot->srgb);
                continue;
            }
            bool known = false;
            for_each_field<MaterialDesc>([&](auto field) { known |= field.name == p.name; });
            if (!known) {
                warn("'" + p.name + "' is not a PBR parameter");
                continue;
            }
            // The field's type decides the shape: try a number, then 2, 3 and 4 components until it reads cleanly.
            const Json candidates[] = {Json(p.value.x), Json::array({p.value.x, p.value.y}), Json::array({p.value.x, p.value.y, p.value.z}),
                                       Json::array({p.value.x, p.value.y, p.value.z, p.value.w})};
            bool applied = false;
            for (const Json& candidate : candidates) {
                Json factors = Json::object();
                factors[p.name] = candidate;
                std::vector<std::string> errors;
                MaterialDesc trial = desc;
                if (from_json(factors, trial, {.resolve = {}, .errors = &errors, .path = "values"}) && errors.empty()) {
                    desc = std::move(trial);
                    applied = true;
                    break;
                }
            }
            if (!applied) warn("'" + p.name + "': the value does not fit the field");
        }
    }
    if (derived.valid()) {
        (void)renderer_.update_material(derived, desc);
        return derived;
    }
    return renderer_.create_material(desc);
}

MaterialHandle MaterialLibrary::get(std::string_view path) {
    if (const auto it = material_index_.find(path); it != material_index_.end()) return materials_[it->second]->handle;
    auto material = std::make_unique<Material>();
    material->path = std::string(path);
    if (!build(*material, true)) return {};
    material_index_.emplace(material->path, materials_.size());
    materials_.push_back(std::move(material));
    return materials_.back()->handle;
}

void MaterialLibrary::update(double now) {
    if (!watch_ || now < next_check_) return;
    next_check_ = now + 0.25;
    update_textures();

    bool any_material_rebound = false;
    for (auto& s : shaders_) {
        const auto current_time = time_of(s->path);
        if (current_time == s->time) continue;
        s->time = current_time;
        const std::string path = s->path;
        const ShaderHandle before = s->handle;
        ShaderHandle old_handle{};
        load_shader(path, true, &old_handle);
        if (s->handle != before) {
            for (auto& m : materials_) {
                if (m->shader == s.get()) {
                    build(*m, false);
                    any_material_rebound = true;
                }
            }
            retire_shader(old_handle);
        }
    }

    for (auto& m : materials_) {
        bool changed = false;
        for (auto& w : m->watched) {
            const auto t = time_of(w.first);
            if (t != w.second) {
                w.second = t;
                changed = true;
            }
        }
        if (changed) {
            build(*m, false);
            any_material_rebound = true;
        }
    }

    if (any_material_rebound && !retired_shaders_.empty()) sweep_retired_shaders();
}

std::vector<MaterialLibrary::Info> MaterialLibrary::loaded() {
    std::vector<Info> out;
    out.reserve(materials_.size());
    for (auto& m : materials_) {
        Info info;
        info.path = m->path;
        info.shader = m->file.shader;
        info.source = m->shader ? &m->shader->source : nullptr;
        info.values = &m->values;
        if (m->shader) {
            info.parameters.reserve(m->shader->source.uniforms.size());
            for (std::size_t i = 0; i < m->shader->source.uniforms.size(); ++i) {
                info.parameters.push_back({&m->shader->source.uniforms[i], m->shader->layout.offsets[i]});
            }
        }
        out.push_back(std::move(info));
    }
    return out;
}

void MaterialLibrary::apply(std::string_view path) {
    const auto it = material_index_.find(path);
    if (it == material_index_.end()) return;
    Material& m = *materials_[it->second];
    m.desc.params = m.values;
    renderer_.update_material(m.handle, m.desc);
    ++revision_;
}

} // namespace astral
