#include <astral/core/jobs.hpp>
#include <astral/render/renderer.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

using namespace astral;

namespace {

float srgb_to_linear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
float linear_to_srgb(float c) { return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f; }

/// The straightforward area filter build_mips started as. Kept here as the specification: the optimized version must
/// produce exactly the same bytes (changing mips would change every lit scene and golden image).
std::vector<std::vector<std::uint8_t>> reference_mips(const ImageData& image) {
    std::vector<std::vector<std::uint8_t>> mips{image.rgba};
    std::uint32_t w = image.width, h = image.height;
    while (w > 1 || h > 1) {
        const std::uint32_t nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
        const std::vector<std::uint8_t>& src = mips.back();
        std::vector<std::uint8_t> dst(std::size_t{nw} * nh * 4);
        const double fx = static_cast<double>(w) / nw, fy = static_cast<double>(h) / nh;
        for (std::uint32_t y = 0; y < nh; ++y) {
            const double y0 = y * fy, y1 = (y + 1) * fy;
            for (std::uint32_t x = 0; x < nw; ++x) {
                const double x0 = x * fx, x1 = (x + 1) * fx;
                for (std::uint32_t c = 0; c < 4; ++c) {
                    double sum = 0, area = 0;
                    for (auto sy = static_cast<std::uint32_t>(y0); sy < h && sy < y1; ++sy) {
                        const double wy = std::min<double>(sy + 1, y1) - std::max<double>(sy, y0);
                        for (auto sx = static_cast<std::uint32_t>(x0); sx < w && sx < x1; ++sx) {
                            const double wx = std::min<double>(sx + 1, x1) - std::max<double>(sx, x0);
                            const float v = src[(std::size_t{sy} * w + sx) * 4 + c] / 255.0f;
                            sum += wx * wy * ((image.srgb && c < 3) ? srgb_to_linear(v) : v);
                            area += wx * wy;
                        }
                    }
                    float avg = static_cast<float>(sum / area);
                    if (image.srgb && c < 3) avg = linear_to_srgb(avg);
                    dst[(std::size_t{y} * nw + x) * 4 + c] = static_cast<std::uint8_t>(std::clamp(avg, 0.0f, 1.0f) * 255.0f + 0.5f);
                }
            }
        }
        mips.push_back(std::move(dst));
        w = nw;
        h = nh;
    }
    return mips;
}

ImageData random_image(std::uint32_t w, std::uint32_t h, bool srgb, unsigned seed) {
    std::mt19937 rng(seed);
    ImageData img{.width = w, .height = h, .rgba = std::vector<std::uint8_t>(std::size_t{w} * h * 4), .srgb = srgb};
    for (std::uint8_t& b : img.rgba) b = static_cast<std::uint8_t>(rng() & 0xFF);
    return img;
}

} // namespace

TEST_CASE("build_mips equals the reference area filter for every shape and colour space") {
    const std::uint32_t sizes[][2] = {{1, 1}, {2, 2}, {4, 2}, {2, 8}, {16, 16}, {3, 3}, {5, 7}, {1, 9}, {33, 17}, {64, 48}, {100, 1}};
    unsigned seed = 1;
    for (const auto& s : sizes) {
        for (const bool srgb : {true, false}) {
            CAPTURE(s[0]);
            CAPTURE(s[1]);
            CAPTURE(srgb);
            const ImageData img = random_image(s[0], s[1], srgb, seed++);
            const auto expected = reference_mips(img);
            const auto actual = build_mips(img);
            REQUIRE(actual.size() == expected.size());
            for (std::size_t level = 0; level < expected.size(); ++level) {
                CAPTURE(level);
                CHECK(actual[level] == expected[level]);
            }
        }
    }
}

TEST_CASE("build_mips with a job system gives the same bytes as without") {
    JobSystem jobs(4);
    const std::uint32_t sizes[][2] = {{256, 256}, {300, 200}, {129, 131}, {512, 64}, {1024, 1024}};
    unsigned seed = 50;
    for (const auto& s : sizes) {
        for (const bool srgb : {true, false}) {
            CAPTURE(s[0]);
            CAPTURE(s[1]);
            const ImageData img = random_image(s[0], s[1], srgb, seed++);
            const auto serial = build_mips(img);
            const auto parallel = build_mips(img, &jobs);
            REQUIRE(parallel.size() == serial.size());
            for (std::size_t level = 0; level < serial.size(); ++level) CHECK(parallel[level] == serial[level]);
        }
    }
}

TEST_CASE("build_mips of a 1024 x 1024 colour image is fast enough to stay out of the way of start-up") {
    const ImageData img = random_image(1024, 1024, true, 99);
    auto timed = [&](const char* label, auto&& build) {
        const auto t0 = std::chrono::steady_clock::now();
        const auto mips = build();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        MESSAGE("1024x1024 sRGB mips, " << label << ": " << ms << " ms");
        CHECK(mips.size() == 11);
        return ms;
    };
    JobSystem jobs(4);
    const double reference = timed("reference (original algorithm)", [&] { return reference_mips(img); });
    const double serial = timed("build_mips", [&] { return build_mips(img); });
    const double parallel = timed("build_mips on 4 workers", [&] { return build_mips(img, &jobs); });
    static_cast<void>(parallel);
    CHECK(serial < reference); // the table-driven filter must not be slower than the original
}
