#pragma once
// Internal to Astral::App: .ashader / .material files as Renderer shaders and materials (ADR-0022).
// Loads on first use, keeps every MaterialHandle stable across hot reloads (the material is updated in
// place), and keeps the last good version when an edited file does not compile.

#include "string_hash.hpp"

#include <astral/app/components.hpp>
#include <astral/render/material_file.hpp>
#include <astral/render/renderer.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
    /// ADR-0055 D5: a copy of `base` with `params` applied. `derived` invalid: creates the copy (invalid result if `base` is
    /// unknown); valid: updates that copy in place and returns it. Unknown names and wrong types are logged and skipped.
    MaterialHandle derive(MaterialHandle base, std::span<const MaterialParam> params, MaterialHandle derived);
    /// Bumps whenever any material is (re)built, edited or gets a texture back; derived materials follow it.
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
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
        std::vector<std::uint32_t>* values = nullptr; ///< Parameter words (ADR-0042).
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
        std::vector<std::uint32_t> values;
        std::vector<UniformValues::Texture> texture_sources;
        std::vector<std::pair<std::string, std::filesystem::file_time_type>> watched;
    };

    Shader* load_shader(const std::string& path, bool reload, ShaderHandle* retired_old = nullptr);
    Shader* install_shader(const std::string& path, const ShaderParse& parsed, Shader* existing, ShaderHandle* retired_old);
    bool build(Material& material, bool create);
    void bind_shader_values(const Material& material, const Shader& shader, const MaterialFile& file, MaterialDesc& desc, UniformValues& values);
    const Shader* build_layered(const Material& material, MaterialFile& file, const MaterialReader& reader, MaterialDesc& desc,
                                UniformValues& values, ShaderHandle& retired_old);
    void apply_uniform_params(const Shader& shader, std::span<const MaterialParam> params, MaterialDesc& desc, const std::string& label);
    void apply_pbr_params(std::span<const MaterialParam> params, MaterialDesc& desc, const std::string& label);
    static bool set_pbr_field(MaterialDesc& desc, const MaterialParam& param);
    bool build_pbr(const MaterialFile& file, MaterialDesc& desc, std::vector<std::string>& errors);
    bool reload_texture(Texture& texture);
    void update_textures();
    void retire_shader(ShaderHandle old_handle);
    void sweep_retired_shaders();
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
    std::uint64_t revision_ = 0;
    std::vector<std::unique_ptr<Shader>> shaders_;
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> shader_index_;
    std::vector<std::unique_ptr<Material>> materials_;
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> material_index_;
    std::unordered_map<std::string, Texture, StringHash, std::equal_to<>> textures_;
    std::vector<ShaderHandle> retired_shaders_;
};

} // namespace astral
