#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace astral {

struct TerrainTileSource {
    std::int32_t x = 0;
    std::int32_t z = 0;
    std::string file; ///< Asset-root relative PNG or cooked tile path.
};

struct TerrainManifest {
    std::uint32_t samples = 0; ///< Samples on each edge, including the shared boundary.
    float spacing_m = 1;
    float min_height_m = 0;
    float max_height_m = 0;
    std::vector<TerrainTileSource> tiles;

    [[nodiscard]] float height_m(std::uint16_t sample) const noexcept;
};

/// Reads source JSON or its cooked binary JSON equivalent.
std::optional<TerrainManifest> read_terrain_manifest(std::span<const std::uint8_t> bytes, std::string* error = nullptr);
/// Decodes a 16-bit grayscale PNG or an ATH1 tile. X varies fastest, and rows advance in +Z.
std::optional<std::vector<std::uint16_t>> read_terrain_tile(std::span<const std::uint8_t> bytes, std::uint32_t samples,
                                                           std::string* error = nullptr);
std::vector<std::uint8_t> write_terrain_tile(std::span<const std::uint16_t> heights);

} // namespace astral
