#pragma once
// CPU-side asset data: decoded images and glTF 2.0 models. No GPU here — these functions are pure
// and thread-safe, so they run on worker threads; AssetManager uploads the results to the Renderer.

#include <astral/core/math.hpp>
#include <astral/render/image.hpp>
#include <astral/render/mesh.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace astral {

/// Decodes PNG / JPEG / TGA / BMP (8-bit) to RGBA8. nullopt + `error` on failure.
std::optional<ImageData> decode_image(std::span<const std::uint8_t> bytes, bool srgb, std::string* error = nullptr);
std::optional<ImageData> load_image(const std::filesystem::path& path, bool srgb, std::string* error = nullptr);

/// A glTF material, with texture slots as indices into ModelData::images (-1 = none).
enum class ModelAlphaMode : std::uint8_t { Opaque, Mask, Blend };

struct ModelSampler {
    std::uint8_t wrap_u = 1, wrap_v = 1; // Clamp, repeat, mirrored repeat.
    std::uint8_t min_filter = 5; // Nearest/linear, then the four mipmapped glTF modes.
    bool mag_linear = true;
    bool operator==(const ModelSampler&) const = default;
};

struct ModelTextureTransform {
    Vec2 scale{1, 1};
    Vec2 offset{};
    float rotation = 0.0f;
    std::uint8_t uv_set = 0;
    bool operator==(const ModelTextureTransform&) const = default;
};

struct ModelMaterial {
    std::string name;
    Vec4 base_color{1, 1, 1, 1};
    float roughness = 1.0f; // glTF defaults
    float metallic = 1.0f;
    Vec3 emissive{};
    float normal_scale = 1.0f;
    int base_color_image = -1;
    int metallic_roughness_image = -1;
    int normal_image = -1;
    int emissive_image = -1;
    int occlusion_image = -1;
    ModelAlphaMode alpha_mode = ModelAlphaMode::Opaque;
    float alpha_cutoff = 0.5f;
    float occlusion_strength = 1.0f;
    bool double_sided = false;
    std::array<ModelTextureTransform, 5> texture_transforms{};
    std::array<ModelSampler, 5> samplers{}; // Color, metallic-roughness, normal, emissive, occlusion.
};

/// One drawable piece: geometry + material index (-1 = default material).
struct ModelPrimitive {
    MeshData mesh;
    int material = -1;
};

struct ModelNode {
    std::string name;
    Vec3 translation{};
    Quat rotation{};
    Vec3 scale{1, 1, 1};
    std::vector<std::uint32_t> primitives; ///< Into ModelData::primitives.
    std::vector<std::uint32_t> children;   ///< Into ModelData::nodes.
};

struct ModelImage {
    ImageData image;      ///< Decoded; `srgb` follows its first use (colour vs data).
    std::string source;   ///< File name or "embedded #N", for messages.
};

/// Everything a glTF file describes that the engine uses.
struct ModelData {
    std::vector<ModelPrimitive> primitives;
    std::vector<ModelMaterial> materials;
    std::vector<ModelImage> images;
    std::vector<ModelNode> nodes;
    std::vector<std::uint32_t> roots;  ///< Root nodes of the default scene.
    std::vector<std::filesystem::path> files; ///< Every file read (the .gltf, buffers, images): hot-reload watches these.
    std::vector<std::string> warnings;        ///< Unsupported features that were skipped.
};

/// Loads a .gltf (JSON + external/embedded buffers and images) or .glb file.
/// Supported: triangle meshes (POSITION, NORMAL, TEXCOORD_0, indices), metallic-roughness materials
/// with base colour / metallic-roughness / normal / emissive textures, KHR_materials_emissive_strength,
/// node hierarchy (TRS or matrix). Missing normals are generated. Skins, animations, morph targets,
/// sparse accessors and cameras are reported in `warnings` and skipped.
std::optional<ModelData> load_gltf(const std::filesystem::path& path, std::string* error = nullptr);

} // namespace astral
