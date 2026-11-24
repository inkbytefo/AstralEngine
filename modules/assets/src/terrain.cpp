#include <astral/assets/terrain.hpp>
#include <astral/core/bytes.hpp>
#include <astral/core/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <set>

#include <stb_image.h>

namespace astral {
namespace {
constexpr std::uint32_t tile_magic = 0x31485441; // ATH1
constexpr std::uint32_t max_samples = 1025;

bool integral_in(const Json& value, double min, double max) {
    const double n = value.as_number();
    return value.is_number() && n >= min && n <= max && std::trunc(n) == n;
}

bool valid_asset_path(const std::string& file) {
    if (file.empty() || file.find_first_of("\\:") != std::string::npos || file.find('\0') != std::string::npos) return false;
    const std::filesystem::path path(file);
    return !path.has_root_path() && std::none_of(path.begin(), path.end(), [](const auto& part) {
        return part.empty() || part == "." || part == "..";
    }) && (path.extension() == ".png" || path.extension() == ".aheight");
}

std::uint32_t png_dimension(std::span<const std::uint8_t> bytes, std::size_t offset) {
    std::uint32_t n = 0;
    for (std::size_t i = offset; i < offset + 4; ++i) n = (n << 8) | bytes[i];
    return n;
}
} // namespace

float TerrainManifest::height_m(std::uint16_t sample) const noexcept {
    return std::lerp(min_height_m, max_height_m, static_cast<float>(sample) / 65535.0f);
}

std::optional<TerrainManifest> read_terrain_manifest(std::span<const std::uint8_t> bytes, std::string* error) {
    const auto fail = [error](const char* message) -> std::optional<TerrainManifest> {
        if (error) *error = message;
        return std::nullopt;
    };
    const auto doc = Json::is_binary(bytes) ? Json::from_binary(bytes, error) :
        Json::parse(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), error);
    if (!doc) return std::nullopt;
    if ((*doc)["format"].as_string() != "astral.terrain" || (*doc)["version"].as_number() != 1) {
        return fail("expected an astral.terrain version 1 manifest");
    }
    if (!integral_in((*doc)["samples"], 5, max_samples)) return fail("samples must be between 5 and 1025");
    TerrainManifest result;
    result.samples = static_cast<std::uint32_t>((*doc)["samples"].as_number());
    const auto intervals = result.samples - 1;
    if ((intervals & (intervals - 1)) != 0) return fail("samples must be a power of two plus one");
    const auto meters = [&](const char* key) {
        return (*doc)[key].as_number(std::numeric_limits<double>::quiet_NaN());
    };
    const double spacing = meters("spacing_m");
    const double low = meters("min_height_m");
    const double high = meters("max_height_m");
    constexpr double max_float = std::numeric_limits<float>::max();
    if (!std::isfinite(spacing) || spacing < std::numeric_limits<float>::min() || spacing > max_float / intervals ||
        !std::isfinite(low) || !std::isfinite(high) || low < -max_float || high > max_float || low > high) {
        return fail("invalid terrain spacing or height range");
    }
    result.spacing_m = static_cast<float>(spacing);
    result.min_height_m = static_cast<float>(low);
    result.max_height_m = static_cast<float>(high);

    const auto& tiles = (*doc)["tiles"].as_array();
    if (tiles.empty()) return fail("terrain must contain at least one tile");
    std::set<std::pair<std::int32_t, std::int32_t>> addresses;
    for (const auto& tile : tiles) {
        if (!integral_in(tile["x"], INT32_MIN, INT32_MAX) || !integral_in(tile["z"], INT32_MIN, INT32_MAX) ||
            !valid_asset_path(tile["file"].as_string())) return fail("invalid terrain tile address or file");
        TerrainTileSource source{static_cast<std::int32_t>(tile["x"].as_number()),
                                 static_cast<std::int32_t>(tile["z"].as_number()), tile["file"].as_string()};
        if (!addresses.emplace(source.x, source.z).second) return fail("duplicate terrain tile address");
        result.tiles.push_back(std::move(source));
    }
    return result;
}

std::optional<std::vector<std::uint16_t>> read_terrain_tile(std::span<const std::uint8_t> bytes, std::uint32_t samples,
                                                           std::string* error) {
    const auto fail = [error](const char* message) -> std::optional<std::vector<std::uint16_t>> {
        if (error) *error = message;
        return std::nullopt;
    };
    if (samples < 5 || samples > max_samples) return fail("invalid terrain sample count");
    const std::size_t count = static_cast<std::size_t>(samples) * samples;
    ByteReader reader(bytes);
    if (reader.u32() == tile_magic) {
        if (reader.remaining() != count * 2) return fail("cooked terrain tile size does not match manifest");
        std::vector<std::uint16_t> result(count);
        for (auto& sample : result) {
            const auto low = reader.u8();
            sample = static_cast<std::uint16_t>(low | (static_cast<unsigned>(reader.u8()) << 8));
        }
        return result;
    }
    constexpr std::uint8_t png_header[]{137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    if (bytes.size() < 26 || bytes.size() > INT32_MAX || !std::equal(std::begin(png_header), std::end(png_header), bytes.begin()) ||
        bytes[24] != 16 || bytes[25] != 0 || png_dimension(bytes, 16) != samples || png_dimension(bytes, 20) != samples) {
        return fail("expected a square 16-bit grayscale PNG matching the terrain sample count");
    }
    int width = 0, height = 0, channels = 0;
    const std::unique_ptr<stbi_us, decltype(&stbi_image_free)> pixels(
        stbi_load_16_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 1), stbi_image_free);
    if (!pixels || width != static_cast<int>(samples) || height != static_cast<int>(samples) || channels != 1) {
        return fail("cannot decode terrain heightmap");
    }
    return std::vector<std::uint16_t>(pixels.get(), pixels.get() + count);
}

std::vector<std::uint8_t> write_terrain_tile(std::span<const std::uint16_t> heights) {
    ByteWriter writer;
    writer.u32(tile_magic);
    for (const auto sample : heights) {
        writer.u8(static_cast<std::uint8_t>(sample));
        writer.u8(static_cast<std::uint8_t>(sample >> 8));
    }
    return writer.take();
}
} // namespace astral
