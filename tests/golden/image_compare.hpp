#pragma once

// Golden image comparison: tolerant per-pixel comparison (software/lavapipe) and perceptual
// luminance SSIM + strong-outlier comparison (hardware GPUs) for RGB8 images.
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
    int pixel_threshold = 8;         // a pixel is "bad" if any channel differs by more than this (0..255)
    double max_bad_fraction = 0.005; // fraction of bad pixels still accepted (GPU/driver noise)
};

struct CompareResult {
    bool same_size = false;
    bool passed = false;
    std::uint64_t bad_pixels = 0;
    int max_channel_diff = 0;
    double mean_abs_diff = 0.0; // over all channels, 0..255
};

struct HardwareTolerance {
    double min_ssim = 0.95;                 // minimum mean luminance SSIM (0..1, 1 = identical)
    int strong_pixel_threshold = 32;        // a pixel is "strongly different" if any channel differs by more than this
    double max_strong_bad_fraction = 0.025; // maximum fraction of strongly different pixels still accepted
};

struct HardwareCompareResult {
    bool same_size = false;
    bool passed = false;
    double mean_ssim = 0.0; // mean luminance SSIM, -1..1 (1 = identical)
    std::uint64_t strong_bad_pixels = 0;
    int max_channel_diff = 0;
    double mean_abs_diff = 0.0; // over all channels, 0..255
};

[[nodiscard]] CompareResult compare(const Image& reference, const Image& actual, const Tolerance& tolerance);

// Mean structural similarity (SSIM) on Rec.709 luminance over sliding 8x8 windows. Returns 0 on size mismatch.
[[nodiscard]] double mean_luminance_ssim(const Image& reference, const Image& actual);

// Hardware-tolerant comparison against a software reference: requires both high structural similarity
// (mean luminance SSIM) and a bounded fraction of strongly different pixels (catches channel swaps / colour bugs).
[[nodiscard]] HardwareCompareResult compare_hardware(const Image& reference, const Image& actual,
                                                     const HardwareTolerance& tolerance);

// Visualises a comparison: dimmed grey copy of the reference with bad pixels in red. Sizes must match.
[[nodiscard]] Image diff_image(const Image& reference, const Image& actual, int pixel_threshold);

} // namespace astral::golden
