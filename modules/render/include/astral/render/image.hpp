#pragma once

#include <cstdint>
#include <vector>

namespace astral {

/// 8-bit RGBA pixels, rows top to bottom. The Renderer builds the mip chain.
/// Pixel layout of ImageData::levels. BC5 keeps two channels (normal map X/Y; Z is rebuilt in the
/// shader), BC7 four; both are 4x4 blocks of 16 bytes (see gpu::texture_data_size).
enum class ImageFormat : std::uint8_t { RGBA8, BC5, BC7 };

struct ImageData {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba{}; ///< width * height * 4 bytes; mips are built on upload.
    bool srgb = true;               ///< Colour (base colour, emissive) = true; data (normals, roughness) = false.
    /// Ready-made mip chain (cooked assets), level 0 first, in `format`. When set, `rgba` is unused.
    ImageFormat format = ImageFormat::RGBA8;
    std::vector<std::vector<std::uint8_t>> levels{};
};

} // namespace astral
