#pragma once
// The cook step: source assets (glTF, PNG, .scene) -> what a shipped game loads (GPU-ready mip
// chains, binary documents). astral-cook runs it; games never need to.
//
//     CookReport r = cook_directory({.source = "content", .output = "build/cooked"});
//     write_pack("build/cooked", "build/game.apak");
//
// Cooked files keep their source path ("props/crate.glb" stays "props/crate.glb"), so the same
// ModelRef / scene works with loose source files, a loose cooked folder and a .apak.

#include <astral/assets/model.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace astral {

/// What a texture holds decides its shipping format.
enum class TextureUse : std::uint8_t {
    Color,  ///< Base colour, emissive: BC7, sRGB.
    Data,   ///< Metallic-roughness and other linear data: BC7.
    Normal, ///< Tangent-space normal map: BC5 (X/Y; the shader rebuilds Z).
};

struct TextureCookOptions {
    bool compress = true;        ///< False: RGBA8 mip chains (bigger, exact).
    std::uint32_t bc7_quality = 1; ///< 0 (fastest) .. 4 (best).
};

/// RGBA8 image -> full mip chain in its shipping format. Pure and thread-safe.
ImageData cook_image(const ImageData& image, TextureUse use, const TextureCookOptions& options = {});
/// Block-compressed levels -> RGBA8 levels (GPUs without BC support). Other images come back as is.
ImageData decompress_image(const ImageData& image);

/// Cooks every image of a loaded glTF for the way its materials use it (in parallel with `jobs`).
void cook_model_images(ModelData& model, const TextureCookOptions& options = {}, JobSystem* jobs = nullptr);

/// The cooked model format ("AMDL"): ModelData with ready mip chains, read without any decoding work.
[[nodiscard]] std::vector<std::uint8_t> write_cooked_model(const ModelData& model);
std::optional<ModelData> read_cooked_model(std::span<const std::uint8_t> data, std::string* error = nullptr);
[[nodiscard]] bool is_cooked_model(std::span<const std::uint8_t> data) noexcept;

struct CookOptions {
    std::filesystem::path source; ///< Folder of source assets.
    std::filesystem::path output; ///< Folder of cooked files (created; stale files from earlier cooks removed).
    TextureCookOptions textures{};
    JobSystem* jobs = nullptr;    ///< Cook assets in parallel.
    Logger* logger = nullptr;
};

struct CookReport {
    std::uint32_t cooked = 0;      ///< Models and documents converted this run.
    std::uint32_t copied = 0;      ///< Other files copied as they are.
    std::uint32_t up_to_date = 0;  ///< Skipped: source (and dependencies) unchanged since the last cook.
    std::uint32_t removed = 0;     ///< Outputs whose source is gone.
    std::vector<std::string> errors;
    [[nodiscard]] bool ok() const noexcept { return errors.empty(); }
};

/// Cooks a folder incrementally: .gltf/.glb -> cooked model (the buffers and images they use are
/// folded in and not copied), .world + "<name>.entities/" -> index + "<name>.cells/*.cell" (ADR-0021),
/// .scene/.prefab/.json -> binary JSON, anything else copied. Files and
/// folders starting with '.' are skipped. A cache in the output folder remembers content hashes.
CookReport cook_directory(const CookOptions& options);

} // namespace astral
