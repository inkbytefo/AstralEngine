#include "material_library.hpp"
#include "json_asset.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/assets/model.hpp>
#include <astral/core/log.hpp>
#include <astral/world/serialize.hpp>

#include <algorithm>
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
    ShaderHandle handle;
    UniformLayout layout;
    if (parsed.source.type == ShaderType::Surface) {
        const SurfaceShaderBuild build = compile_surface_shader(parsed.source, path);
        if (!build.ok()) {
            std::string all;
            for (const ShaderError& e : build.errors) all += "\n  " + path + ":" + std::to_string(e.line) + ": " + e.message;
            return fail(all);
        }
        handle = renderer_.create_shader(build, path);
        layout = build.layout;
    } else if (parsed.source.type == ShaderType::Post || parsed.source.type == ShaderType::Sky ||
               parsed.source.type == ShaderType::Volume || parsed.source.type == ShaderType::Particle) {
        const ScreenShaderBuild build = compile_screen_shader(parsed.source, path);
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

bool MaterialLibrary::build_pbr(const MaterialFile& file, MaterialDesc& desc, std::vector<std::string>& errors) {
    struct TextureSlot {
        std::string_view name;
        TextureHandle& handle;
        bool srgb;
    };
    TextureSlot slots[] = {
        {"base_color_texture", desc.base_color_texture, true},
        {"metallic_roughness_texture", desc.metallic_roughness_texture, false},
        {"normal_texture", desc.normal_texture, false},
        {"emissive_texture", desc.emissive_texture, true},
        {"detail_albedo_texture", desc.detail.albedo_texture, true},
        {"detail_normal_texture", desc.detail.normal_texture, false},
        {"detail_height_texture", desc.detail.height_texture, false},
    };
    Json factors = Json::object();
    for (const auto& [name, value] : file.values) {
        const auto slot = std::find_if(std::begin(slots), std::end(slots), [&](const auto& s) { return s.name == name; });
        if (slot != std::end(slots)) {
            if (!value.is_string()) errors.push_back(name + ": expected an asset path");
            else if (!value.as_string().empty()) slot->handle = texture(value.as_string(), slot->srgb);
            continue;
        }
        bool known = false;
        for_each_field<MaterialDesc>([&](auto field) { known |= field.name == name; });
        if (!known) errors.push_back(name + ": unknown PBR parameter");
        else factors[name] = value;
    }
    const bool parsed = from_json(factors, desc, {.resolve = {}, .errors = &errors, .path = "values"});
    return parsed && errors.empty();
}

/// (Re)builds a material from its files. On failure nothing changes (false).
bool MaterialLibrary::build(Material& material, bool create) {
    const MaterialReader reader = [this](std::string_view path, std::string* error) { return read_document(path, error); };
    MaterialFile file = resolve_material(material.path, reader);
    if (!file.ok()) {
        for (const std::string& e : file.errors) log_error("material " + material.path, e);
        return false;
    }
    const Shader* shader = nullptr;
    UniformValues values;
    MaterialDesc desc;
    if (file.standard_pbr) {
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
    return true;
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
}

} // namespace astral
