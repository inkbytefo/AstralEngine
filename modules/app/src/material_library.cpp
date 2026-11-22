#include "material_library.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/assets/model.hpp>
#include <astral/core/log.hpp>

#include <algorithm>
#include <system_error>

namespace astral {

MaterialLibrary::MaterialLibrary(Renderer& renderer, AssetManager& assets, std::filesystem::path root, bool watch, Logger& log)
    : renderer_(renderer), assets_(assets), root_(std::move(root)), watch_(watch), log_(log) {}

MaterialLibrary::~MaterialLibrary() {
    for (auto& m : materials_) renderer_.destroy(m->handle);
    for (auto& s : shaders_) renderer_.destroy(s->handle);
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
    if (Json::is_binary(*bytes)) return Json::from_binary(*bytes, error);
    return Json::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), error);
}

TextureHandle MaterialLibrary::texture(const std::string& path, bool srgb) {
    const std::string key = path + (srgb ? "#srgb" : "#linear");
    if (auto it = textures_.find(key); it != textures_.end()) return it->second;
    TextureHandle handle;
    std::string error;
    if (auto bytes = assets_.read(path, &error)) {
        if (auto image = decode_image(*bytes, srgb, &error)) handle = renderer_.create_texture(*image);
    }
    if (!handle.valid()) log_error("texture " + path, error.empty() ? "cannot create texture" : error);
    textures_.emplace(key, handle); // failures are remembered: logged once
    return handle;
}

/// Compiles a .ashader. With `reload`, an existing entry gets its new pipelines only if the compile
/// succeeds (the old handle is returned through a replaced entry for the caller to destroy).
MaterialLibrary::Shader* MaterialLibrary::load_shader(const std::string& path, bool reload) {
    Shader* existing = nullptr;
    for (auto& s : shaders_) {
        if (s->path == path) existing = s.get();
    }
    if (existing && !reload) return existing;

    auto fail = [&](const std::string& message) -> Shader* {
        log_error("shader " + path, message);
        return existing; // the last good version (null on first load)
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
    } else if (parsed.source.type == ShaderType::Post || parsed.source.type == ShaderType::Sky) {
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
        const ShaderHandle old = existing->handle;
        existing->handle = handle;
        existing->source = parsed.source;
        existing->layout = layout;
        existing->time = time_of(path);
        renderer_.destroy(old); // after the caller has pointed its materials at the new one
        return existing;
    }
    auto entry = std::make_unique<Shader>();
    entry->path = path;
    entry->handle = handle;
    entry->source = parsed.source;
    entry->layout = layout;
    entry->time = time_of(path);
    shaders_.push_back(std::move(entry));
    return shaders_.back().get();
}

/// (Re)builds a material from its files. On failure nothing changes (false).
bool MaterialLibrary::build(Material& material, bool create) {
    const MaterialReader reader = [this](std::string_view path, std::string* error) { return read_document(path, error); };
    MaterialFile file = resolve_material(material.path, reader);
    if (!file.ok()) {
        for (const std::string& e : file.errors) log_error("material " + material.path, e);
        return false;
    }
    const Shader* shader = load_shader(file.shader, false);
    if (!shader) return false;
    const UniformValues values = evaluate_uniforms(shader->source, shader->layout, file);
    for (const std::string& w : values.warnings) log_.log(LogLevel::Warn, "material " + material.path + ": " + w);

    MaterialDesc desc;
    desc.shader = shader->handle;
    desc.params = values.params;
    for (const UniformValues::Texture& t : values.textures) {
        desc.textures.push_back({t.offset, texture(t.path, t.srgb), t.normal});
    }
    if (create) material.handle = renderer_.create_material(desc);
    else renderer_.update_material(material.handle, desc);
    if (!material.handle.valid()) return false;
    material.file = std::move(file);
    material.shader = shader;
    material.desc = std::move(desc);
    material.values = material.desc.params;
    material.watched.clear();
    for (const std::string& dep : material.file.dependencies) material.watched.emplace_back(dep, time_of(dep));
    return true;
}

MaterialHandle MaterialLibrary::get(std::string_view path) {
    for (auto& m : materials_) {
        if (m->path == path) return m->handle;
    }
    auto material = std::make_unique<Material>();
    material->path = std::string(path);
    if (!build(*material, true)) return {};
    materials_.push_back(std::move(material));
    return materials_.back()->handle;
}

void MaterialLibrary::update(double now) {
    if (!watch_ || now < next_check_) return;
    next_check_ = now + 0.25;
    // Shaders first: a new pipeline set is made, then every material using it is pointed at it.
    for (auto& s : shaders_) {
        if (time_of(s->path) == s->time) continue;
        s->time = time_of(s->path); // a failed compile is not retried until the file changes again
        const std::string path = s->path;
        const ShaderHandle before = s->handle;
        load_shader(path, true);
        if (s->handle != before) {
            for (auto& m : materials_) {
                if (m->shader == s.get()) build(*m, false);
            }
        }
    }
    for (auto& m : materials_) {
        const bool changed = std::any_of(m->watched.begin(), m->watched.end(),
                                         [&](const auto& w) { return time_of(w.first) != w.second; });
        if (!changed) continue;
        for (auto& w : m->watched) w.second = time_of(w.first); // do not retry a broken edit every check
        build(*m, false);
    }
}

std::vector<MaterialLibrary::Info> MaterialLibrary::loaded() {
    std::vector<Info> out;
    for (auto& m : materials_) {
        Info info;
        info.path = m->path;
        info.shader = m->file.shader;
        info.source = m->shader ? &m->shader->source : nullptr;
        info.values = &m->values;
        if (m->shader) {
            for (std::size_t i = 0; i < m->shader->source.uniforms.size(); ++i) {
                info.parameters.push_back({&m->shader->source.uniforms[i], m->shader->layout.offsets[i]});
            }
        }
        out.push_back(std::move(info));
    }
    return out;
}

void MaterialLibrary::apply(std::string_view path) {
    for (auto& m : materials_) {
        if (m->path != path) continue;
        m->desc.params = m->values;
        renderer_.update_material(m->handle, m->desc);
    }
}

} // namespace astral
