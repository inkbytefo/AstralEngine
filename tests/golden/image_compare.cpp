#include "image_compare.hpp"

#include <algorithm>
#include <cstdlib>

namespace astral::golden {

namespace {

int pixel_diff(const Image& a, const Image& b, std::size_t pixel) {
    int worst = 0;
    for (std::size_t c = 0; c < 3; ++c) {
        worst = std::max(worst, std::abs(int{a.rgb[pixel * 3 + c]} - int{b.rgb[pixel * 3 + c]}));
    }
    return worst;
}

} // namespace

CompareResult compare(const Image& reference, const Image& actual, const Tolerance& tolerance) {
    CompareResult r;
    const std::size_t pixels = std::size_t{reference.width} * reference.height;
    r.same_size = reference.width == actual.width && reference.height == actual.height &&
                  reference.rgb.size() == pixels * 3 && actual.rgb.size() == pixels * 3;
    if (!r.same_size) return r;

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
