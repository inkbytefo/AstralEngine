#include "image_compare.hpp"

#include <doctest/doctest.h>

using astral::golden::compare;
using astral::golden::diff_image;
using astral::golden::Image;
using astral::golden::Tolerance;

namespace {

Image solid(std::uint32_t w, std::uint32_t h, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    Image img{w, h, {}};
    for (std::uint32_t i = 0; i < w * h; ++i) {
        img.rgb.push_back(r);
        img.rgb.push_back(g);
        img.rgb.push_back(b);
    }
    return img;
}

} // namespace

TEST_CASE("identical images match with zero error") {
    const Image a = solid(4, 4, 10, 20, 30);
    const auto r = compare(a, a, {});
    CHECK(r.same_size);
    CHECK(r.passed);
    CHECK(r.bad_pixels == 0);
    CHECK(r.max_channel_diff == 0);
}

TEST_CASE("different sizes never match") {
    const auto r = compare(solid(4, 4, 0, 0, 0), solid(4, 3, 0, 0, 0), {});
    CHECK_FALSE(r.same_size);
    CHECK_FALSE(r.passed);
}

TEST_CASE("small per-channel noise below the threshold passes") {
    const Image a = solid(8, 8, 100, 100, 100);
    Image b = a;
    b.rgb[0] = 103; // off by 3 on one channel
    const auto r = compare(a, b, {.pixel_threshold = 8, .max_bad_fraction = 0.0});
    CHECK(r.passed);
    CHECK(r.bad_pixels == 0);
    CHECK(r.max_channel_diff == 3);
}

TEST_CASE("a few bad pixels pass only within the allowed fraction") {
    const Image a = solid(10, 10, 0, 0, 0);
    Image b = a;
    b.rgb[0] = 255;      // pixel 0
    b.rgb[3 * 5 + 1] = 200; // pixel 5
    const Tolerance strict{.pixel_threshold = 8, .max_bad_fraction = 0.01}; // 1 of 100
    const Tolerance loose{.pixel_threshold = 8, .max_bad_fraction = 0.02};  // 2 of 100
    CHECK(compare(a, b, strict).bad_pixels == 2);
    CHECK_FALSE(compare(a, b, strict).passed);
    CHECK(compare(a, b, loose).passed);
}

TEST_CASE("diff image marks bad pixels red and dims the rest") {
    const Image a = solid(2, 1, 200, 200, 200);
    Image b = a;
    b.rgb[3] = 0; // second pixel differs
    const Image d = diff_image(a, b, 8);
    REQUIRE(d.rgb.size() == 6);
    CHECK(d.rgb[0] < 200);         // first pixel dimmed copy of the reference
    CHECK(d.rgb[0] == d.rgb[1]);   // grey
    CHECK(d.rgb[3] == 255);        // second pixel red
    CHECK(d.rgb[4] == 0);
    CHECK(d.rgb[5] == 0);
}
