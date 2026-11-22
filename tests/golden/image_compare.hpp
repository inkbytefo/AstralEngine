#pragma once

// Golden image comparison: tolerant per-pixel comparison of two RGB8 images.
// Test infrastructure only (not part of the engine's public API).

#include <cstdint>
#include <vector>

namespace astral::golden {

struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgb; // width * height * 3, row-major, top-down
};

struct Tolerance {
    int pixel_threshold = 8;        // a pixel is "bad" if any channel differs by more than this (0..255)
    double max_bad_fraction = 0.005; // fraction of bad pixels still accepted (GPU/driver noise)
};

struct CompareResult {
    bool same_size = false;
    bool passed = false;
    std::uint64_t bad_pixels = 0;
    int max_channel_diff = 0;
    double mean_abs_diff = 0.0; // over all channels, 0..255
};

[[nodiscard]] CompareResult compare(const Image& reference, const Image& actual, const Tolerance& tolerance);

// Visualises a comparison: dimmed grey copy of the reference with bad pixels in red. Sizes must match.
[[nodiscard]] Image diff_image(const Image& reference, const Image& actual, int pixel_threshold);

} // namespace astral::golden
