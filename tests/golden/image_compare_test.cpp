#include "device_info.hpp"
#include "image_compare.hpp"
#include "image_io.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>

using astral::golden::CaptureDevice;
using astral::golden::classify_device;
using astral::golden::compare;
using astral::golden::compare_hardware;
using astral::golden::DeviceClass;
using astral::golden::diff_image;
using astral::golden::format_device;
using astral::golden::HardwareTolerance;
using astral::golden::Image;
using astral::golden::load_image;
using astral::golden::mean_luminance_ssim;
using astral::golden::parse_capture_device;
using astral::golden::Tolerance;

namespace {

Image solid(std::uint32_t w, std::uint32_t h, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    Image img{w, h, {}};
    img.rgb.reserve(std::size_t{w} * h * 3);
    for (std::uint32_t i = 0; i < w * h; ++i) {
        img.rgb.push_back(r);
        img.rgb.push_back(g);
        img.rgb.push_back(b);
    }
    return img;
}

Image swap_rb(const Image& src) {
    Image out = src;
    const std::size_t pixels = std::size_t{src.width} * src.height;
    for (std::size_t p = 0; p < pixels; ++p) std::swap(out.rgb[p * 3 + 0], out.rgb[p * 3 + 2]);
    return out;
}

Image shift_horizontal(const Image& src, double fraction) {
    Image out{src.width, src.height, std::vector<std::uint8_t>(src.rgb.size(), 0)};
    const std::uint32_t dx = std::max(1u, static_cast<std::uint32_t>(static_cast<double>(src.width) * fraction));
    for (std::uint32_t y = 0; y < src.height; ++y) {
        for (std::uint32_t x = 0; x < src.width; ++x) {
            const std::uint32_t sx = (x + src.width - dx) % src.width;
            const std::size_t dst = (std::size_t{y} * src.width + x) * 3;
            const std::size_t src_i = (std::size_t{y} * src.width + sx) * 3;
            out.rgb[dst + 0] = src.rgb[src_i + 0];
            out.rgb[dst + 1] = src.rgb[src_i + 1];
            out.rgb[dst + 2] = src.rgb[src_i + 2];
        }
    }
    return out;
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
    const auto hw = compare_hardware(solid(4, 4, 0, 0, 0), solid(4, 3, 0, 0, 0), {});
    CHECK_FALSE(hw.same_size);
    CHECK_FALSE(hw.passed);
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
    b.rgb[0] = 255;         // pixel 0
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
    CHECK(d.rgb[0] < 200);       // first pixel dimmed copy of the reference
    CHECK(d.rgb[0] == d.rgb[1]); // grey
    CHECK(d.rgb[3] == 255);      // second pixel red
    CHECK(d.rgb[4] == 0);
    CHECK(d.rgb[5] == 0);
}

TEST_CASE("luminance SSIM is 1 for identical images and low for inverted images") {
    Image a{16, 16, {}};
    a.rgb.resize(16 * 16 * 3);
    for (std::uint32_t y = 0; y < 16; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            const auto v = static_cast<std::uint8_t>((x * 16 + y * 8) & 0xff);
            const std::size_t i = (std::size_t{y} * 16 + x) * 3;
            a.rgb[i + 0] = v;
            a.rgb[i + 1] = v;
            a.rgb[i + 2] = v;
        }
    }
    Image inv = a;
    for (auto& c : inv.rgb) c = static_cast<std::uint8_t>(255 - c);
    CHECK(mean_luminance_ssim(a, a) == doctest::Approx(1.0).epsilon(1e-6));
    CHECK(mean_luminance_ssim(a, inv) < 0.1);
}

TEST_CASE("hardware metric rejects gross breakage of a reference image") {
    const std::string path = std::string(ASTRAL_GOLDEN_REFERENCES_DIR) + "/scene_viewer.png";
    const auto ref = load_image(path.c_str());
    REQUIRE(ref.has_value());

    const HardwareTolerance tol{};

    // Identical image passes with perfect SSIM.
    const auto identical = compare_hardware(*ref, *ref, tol);
    CHECK(identical.same_size);
    CHECK(identical.passed);
    CHECK(identical.mean_ssim == doctest::Approx(1.0).epsilon(1e-6));
    CHECK(identical.strong_bad_pixels == 0);

    // Mild uniform drift (simulating minor GPU rounding) stays well within tolerance.
    Image mild = *ref;
    for (auto& c : mild.rgb) c = static_cast<std::uint8_t>(std::min(255, int{c} + 4));
    CHECK(compare_hardware(*ref, mild, tol).passed);

    // Gross breakage 1: black image.
    const Image black = solid(ref->width, ref->height, 0, 0, 0);
    const auto black_res = compare_hardware(*ref, black, tol);
    CHECK_FALSE(black_res.passed);
    CHECK(black_res.mean_ssim < 0.1);

    // Gross breakage 2: swapped R and B colour channels (luminance SSIM stays high, strong pixel diff catches it).
    const Image swapped = swap_rb(*ref);
    const auto swap_res = compare_hardware(*ref, swapped, tol);
    CHECK_FALSE(swap_res.passed);
    CHECK(swap_res.strong_bad_pixels > 0);

    // Gross breakage 3: 10% horizontal shift.
    const Image shifted = shift_horizontal(*ref, 0.10);
    const auto shift_res = compare_hardware(*ref, shifted, tol);
    CHECK_FALSE(shift_res.passed);
    CHECK(shift_res.mean_ssim < tol.min_ssim);
}

TEST_CASE("capture device output is parsed and classified into lavapipe, other software, and hardware") {
    const auto lvp = parse_capture_device(
        "some log\ncapture device: name=llvmpipe (LLVM 17.0.6, 256 bits) | vendor=0x10005 | device=0x0000 | driver=13 | "
        "driver_name=llvmpipe | type=cpu\nwrote out.ppm\n");
    REQUIRE(lvp.has_value());
    CHECK(lvp->name == "llvmpipe (LLVM 17.0.6, 256 bits)");
    CHECK(lvp->vendor_id == 0x10005u);
    CHECK(lvp->driver_id == 13u);
    CHECK(lvp->driver_name == "llvmpipe");
    CHECK(lvp->type == "cpu");
    CHECK(classify_device(*lvp) == DeviceClass::Lavapipe);
    CHECK(format_device(*lvp).find("0x10005") != std::string::npos);

    const auto rtx = parse_capture_device(
        "capture device: name=NVIDIA GeForce RTX 3060 | vendor=0x10de | device=0x2504 | driver=4 | "
        "driver_name=NVIDIA | type=discrete\n");
    REQUIRE(rtx.has_value());
    CHECK(rtx->vendor_id == 0x10deu);
    CHECK(rtx->driver_id == 4u);
    CHECK(classify_device(*rtx) == DeviceClass::Hardware);

    const auto swift = parse_capture_device(
        "capture device: name=SwiftShader Device (Subzero) | vendor=0x1ae0 | device=0xc0de | driver=10 | "
        "driver_name=SwiftShader driver | type=cpu\n");
    REQUIRE(swift.has_value());
    CHECK(classify_device(*swift) == DeviceClass::OtherSoftware);

    CHECK_FALSE(parse_capture_device("wrote out.ppm\n").has_value());
    CHECK(classify_device(CaptureDevice{}) == DeviceClass::Unknown);
}
