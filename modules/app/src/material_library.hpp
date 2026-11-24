#pragma once
// Internal to Astral::App: .ashader / .material files as Renderer shaders and materials (ADR-0022).
// Loads on first use, keeps every MaterialHandle stable across hot reloads (the material is updated in
// place), and keeps the last good version when an edited file does not compile.

#include <astral/render/material_file.hpp>
#include <astral/render/renderer.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace astral {

class AssetManager;
class Logger;

class MaterialLibrary {
public:
    /// `watch`: poll the files under `root` for changes (hot reload; not for packs).
    MaterialLibrary(Renderer& renderer, AssetManager& assets, std::filesystem::path root, bool watch, Logger& log);
    ~MaterialLibrary();
    MaterialLibrary(const MaterialLibrary&) = delete;
    MaterialLibrary& operator=(const MaterialLibrary&) = delete;

    /// The material for a .material path, loaded on first use; invalid (and logged) if it cannot be built.
    MaterialHandle get(std::string_view path);
    /// Hot reload: checks the files every 0.25 s of `now` (seconds).
    void update(double now);

    /// For tools: a loaded material's uniforms and live values (non-texture ones are editable).
    struct Parameter {
        const ShaderUniform* uniform = nullptr;
        std::uint32_t offset = 0; ///< Word in `values`.
    };
    struct Info {
        std::string path;
        std::string shader;
        const ShaderSource* source = nullptr;
        std::vector<Parameter> parameters;
        std::vector<float>* values = nullptr;
    };
    [[nodiscard]] std::vector<Info> loaded();
    /// Applies edited `values` of a material (tools); runtime only, the file is not written.
    void apply(std::string_view path);

private:
    struct Shader {
        std::string path;
        ShaderHandle handle;
        ShaderSource source;
        UniformLayout layout;
        std::filesystem::file_time_type time{};
    };
    struct Texture {
        std::string path;
        bool srgb = false;
        TextureHandle handle;
        std::filesystem::file_time_type time{};
    };
    struct Material {
        std::string path;
        MaterialHandle handle;
        MaterialFile file;
        const Shader* shader = nullptr;
        MaterialDesc desc;
        std::vector<float> values;
        std::vector<UniformValues::Texture> texture_sources;
        std::vector<std::pair<std::string, std::filesystem::file_time_type>> watched;
    };

    Shader* load_shader(const std::string& path, bool reload);
    bool build(Material& material, bool create);
    bool build_pbr(const MaterialFile& file, MaterialDesc& desc, std::vector<std::string>& errors);
    bool reload_texture(Texture& texture);
    void update_textures();
    TextureHandle texture(const std::string& path, bool srgb);
    std::optional<Json> read_document(std::string_view path, std::string* error);
    std::filesystem::file_time_type time_of(const std::string& path) const;
    void log_error(const std::string& what, const std::string& message);

    Renderer& renderer_;
    AssetManager& assets_;
    std::filesystem::path root_;
    bool watch_;
    Logger& log_;
    double next_check_ = 0.0;
    std::vector<std::unique_ptr<Shader>> shaders_;
    std::vector<std::unique_ptr<Material>> materials_;
    std::unordered_map<std::string, Texture> textures_;
};

} // namespace astral
