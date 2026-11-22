#include "image_compare.hpp"

#include <algorithm>
#include <cstdlib>
#include <span>

namespace astral::golden {

namespace {

bool images_match_size(const Image& a, const Image& b) noexcept {
    const std::size_t pixels = std::size_t{a.width} * a.height;
    return a.width == b.width && a.height == b.height && a.rgb.size() == pixels * 3 && b.rgb.size() == pixels * 3;
}

int pixel_diff(const Image& a, const Image& b, std::size_t pixel) noexcept {
    int worst = 0;
    for (std::size_t c = 0; c < 3; ++c) {
        worst = std::max(worst, std::abs(int{a.rgb[pixel * 3 + c]} - int{b.rgb[pixel * 3 + c]}));
    }
    return worst;
}

// Separable 2D box sum over `win x win` windows with stride 1 (exact, no running-subtraction drift).
std::vector<double> box_sums(std::span<const double> src, std::uint32_t width, std::uint32_t height,
                             std::uint32_t win) {
    const std::uint32_t out_w = width - win + 1;
    const std::uint32_t out_h = height - win + 1;
    std::vector<double> horiz(std::size_t{out_w} * height, 0.0);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::size_t src_row = std::size_t{y} * width;
        const std::size_t dst_row = std::size_t{y} * out_w;
        for (std::uint32_t x = 0; x < out_w; ++x) {
            double s = 0.0;
            for (std::uint32_t k = 0; k < win; ++k) s += src[src_row + x + k];
            horiz[dst_row + x] = s;
        }
    }
    std::vector<double> out(std::size_t{out_w} * out_h, 0.0);
    for (std::uint32_t y = 0; y < out_h; ++y) {
        const std::size_t dst_row = std::size_t{y} * out_w;
        for (std::uint32_t k = 0; k < win; ++k) {
            const std::size_t src_row = std::size_t{y + k} * out_w;
            for (std::uint32_t x = 0; x < out_w; ++x) out[dst_row + x] += horiz[src_row + x];
        }
    }
    return out;
}

} // namespace

CompareResult compare(const Image& reference, const Image& actual, const Tolerance& tolerance) {
    CompareResult r;
    r.same_size = images_match_size(reference, actual);
    if (!r.same_size) return r;

    const std::size_t pixels = std::size_t{reference.width} * reference.height;
    std::uint64_t sum = 0;
    for (std::size_t p = 0; p < pixels; ++p) {
        const int d = pixel_diff(reference, actual, p);
        r.max_channel_diff = std::max(r.max_channel_diff, d);
        if (d > tolerance.pixel_threshold) ++r.bad_pixels;
        for (std::size_t c = 0; c < 3; ++c) {
            sum += static_cast<std::uint64_t>(std::abs(int{reference.rgb[p * 3 + c]} - int{actual.rgb[p * 3 + c]}));
        }
    }
    if (pixels > 0) {
        r.mean_abs_diff = static_cast<double>(sum) / static_cast<double>(pixels * 3);
        r.passed = static_cast<double>(r.bad_pixels) <= tolerance.max_bad_fraction * static_cast<double>(pixels);
    } else {
        r.passed = true;
    }
    return r;
}

double mean_luminance_ssim(const Image& reference, const Image& actual) {
    if (!images_match_size(reference, actual)) return 0.0;
    if (reference.width == 0 || reference.height == 0) return 1.0;

    const std::size_t pixels = std::size_t{reference.width} * reference.height;
    std::vector<double> la(pixels);
    std::vector<double> lb(pixels);
    std::vector<double> laa(pixels);
    std::vector<double> lbb(pixels);
    std::vector<double> lab(pixels);
    for (std::size_t p = 0; p < pixels; ++p) {
        const double a = 0.2126 * reference.rgb[p * 3 + 0] + 0.7152 * reference.rgb[p * 3 + 1] +
                         0.0722 * reference.rgb[p * 3 + 2];
        const double b =
            0.2126 * actual.rgb[p * 3 + 0] + 0.7152 * actual.rgb[p * 3 + 1] + 0.0722 * actual.rgb[p * 3 + 2];
        la[p] = a;
        lb[p] = b;
        laa[p] = a * a;
        lbb[p] = b * b;
        lab[p] = a * b;
    }

    const std::uint32_t win = std::min({std::uint32_t{8}, reference.width, reference.height});
    const auto sa = box_sums(la, reference.width, reference.height, win);
    const auto sb = box_sums(lb, reference.width, reference.height, win);
    const auto saa = box_sums(laa, reference.width, reference.height, win);
    const auto sbb = box_sums(lbb, reference.width, reference.height, win);
    const auto sab = box_sums(lab, reference.width, reference.height, win);

    // Standard Wang et al. (2004) stability constants for 8-bit dynamic range L = 255.
    constexpr double c1 = (0.01 * 255.0) * (0.01 * 255.0);
    constexpr double c2 = (0.03 * 255.0) * (0.03 * 255.0);
    const double inv_n = 1.0 / static_cast<double>(std::size_t{win} * win);

    double total = 0.0;
    for (std::size_t i = 0; i < sa.size(); ++i) {
        const double mx = sa[i] * inv_n;
        const double my = sb[i] * inv_n;
        const double vx = std::max(0.0, saa[i] * inv_n - mx * mx);
        const double vy = std::max(0.0, sbb[i] * inv_n - my * my);
        const double vxy = sab[i] * inv_n - mx * my;
        const double num = (2.0 * mx * my + c1) * (2.0 * vxy + c2);
        const double den = (mx * mx + my * my + c1) * (vx + vy + c2);
        total += std::clamp(num / den, -1.0, 1.0);
    }
    return total / static_cast<double>(sa.size());
}

HardwareCompareResult compare_hardware(const Image& reference, const Image& actual,
                                       const HardwareTolerance& tolerance) {
    HardwareCompareResult r;
    r.same_size = images_match_size(reference, actual);
    if (!r.same_size) return r;

    const std::size_t pixels = std::size_t{reference.width} * reference.height;
    std::uint64_t sum = 0;
    for (std::size_t p = 0; p < pixels; ++p) {
        const int d = pixel_diff(reference, actual, p);
        r.max_channel_diff = std::max(r.max_channel_diff, d);
        if (d > tolerance.strong_pixel_threshold) ++r.strong_bad_pixels;
        for (std::size_t c = 0; c < 3; ++c) {
            sum += static_cast<std::uint64_t>(std::abs(int{reference.rgb[p * 3 + c]} - int{actual.rgb[p * 3 + c]}));
        }
    }
    r.mean_ssim = mean_luminance_ssim(reference, actual);
    if (pixels > 0) {
        r.mean_abs_diff = static_cast<double>(sum) / static_cast<double>(pixels * 3);
        const bool ssim_ok = r.mean_ssim >= tolerance.min_ssim;
        const bool strong_ok =
            static_cast<double>(r.strong_bad_pixels) <= tolerance.max_strong_bad_fraction * static_cast<double>(pixels);
        r.passed = ssim_ok && strong_ok;
    } else {
        r.passed = true;
    }
    return r;
}

Image diff_image(const Image& reference, const Image& actual, int pixel_threshold) {
    Image out{reference.width, reference.height, {}};
    const std::size_t pixels = std::size_t{reference.width} * reference.height;
    out.rgb.resize(pixels * 3);
    for (std::size_t p = 0; p < pixels; ++p) {
        if (pixel_diff(reference, actual, p) > pixel_threshold) {
            out.rgb[p * 3 + 0] = 255;
            out.rgb[p * 3 + 1] = 0;
            out.rgb[p * 3 + 2] = 0;
            continue;
        }
        const int luma = (int{reference.rgb[p * 3]} * 3 + int{reference.rgb[p * 3 + 1]} * 6 + int{reference.rgb[p * 3 + 2]}) / 10;
        const auto grey = static_cast<std::uint8_t>(luma / 3);
        out.rgb[p * 3 + 0] = grey;
        out.rgb[p * 3 + 1] = grey;
        out.rgb[p * 3 + 2] = grey;
    }
    return out;
}

} // namespace astral::golden
