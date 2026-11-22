// astral-golden: compares a captured frame with a stored reference image.
//
//   astral-golden <actual.ppm|png> <reference.png> [--diff out.png] [--threshold N] [--max-bad F] [--update]
//
// Exit 0 when the images match within tolerance. With --update (or a missing reference and
// ASTRAL_UPDATE_GOLDEN=1) the actual image is written as the new reference instead.

#include "image_compare.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <stb_image.h>
#include <stb_image_write.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

using namespace astral::golden;

namespace {

std::optional<Image> load(const char* path) {
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path, &w, &h, &n, 3);
    if (!data) return std::nullopt;
    Image img{static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), {}};
    img.rgb.assign(data, data + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3);
    stbi_image_free(data);
    return img;
}

bool save_png(const char* path, const Image& img) {
    return stbi_write_png(path, static_cast<int>(img.width), static_cast<int>(img.height), 3, img.rgb.data(),
                          static_cast<int>(img.width * 3)) != 0;
}

std::optional<std::string> env_var(const char* name) {
#if defined(_WIN32)
    std::size_t len = 0;
    if (getenv_s(&len, nullptr, 0, name) != 0 || len == 0) return std::nullopt;
    std::string value(len, '\0');
    if (getenv_s(&len, value.data(), value.size(), name) != 0 || len == 0) return std::nullopt;
    value.resize(len - 1);
    return value;
#else
    const char* value = std::getenv(name);
    return value ? std::optional<std::string>(value) : std::nullopt;
#endif
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: astral-golden <actual> <reference.png> [--diff out.png] [--threshold N] "
                             "[--max-bad F] [--update]\n");
        return 2;
    }
    const char* actual_path = argv[1];
    const char* reference_path = argv[2];
    const char* diff_path = nullptr;
    bool update = false;
    Tolerance tol;
    for (int i = 3; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : ""; };
        if (!std::strcmp(argv[i], "--diff")) diff_path = next();
        else if (!std::strcmp(argv[i], "--threshold")) tol.pixel_threshold = std::atoi(next());
        else if (!std::strcmp(argv[i], "--max-bad")) tol.max_bad_fraction = std::atof(next());
        else if (!std::strcmp(argv[i], "--update")) update = true;
    }
    if (const auto env = env_var("ASTRAL_UPDATE_GOLDEN"); env && *env != "0") update = true;

    const auto actual = load(actual_path);
    if (!actual) {
        std::fprintf(stderr, "cannot read captured image %s\n", actual_path);
        return 1;
    }
    const auto reference = load(reference_path);
    if (update || !reference) {
        if (!update) {
            std::fprintf(stderr, "no reference %s (run with ASTRAL_UPDATE_GOLDEN=1 to create it)\n", reference_path);
            return 1;
        }
        if (!save_png(reference_path, *actual)) {
            std::fprintf(stderr, "cannot write reference %s\n", reference_path);
            return 1;
        }
        std::printf("updated reference %s\n", reference_path);
        return 0;
    }

    const CompareResult r = compare(*reference, *actual, tol);
    if (!r.same_size) {
        std::fprintf(stderr, "size mismatch: reference %ux%u, actual %ux%u\n", reference->width, reference->height,
                     actual->width, actual->height);
        return 1;
    }
    const double total = static_cast<double>(reference->width) * reference->height;
    std::printf("%s: bad pixels %llu (%.3f%%, allowed %.3f%%), max diff %d, mean diff %.3f\n",
                r.passed ? "PASS" : "FAIL", static_cast<unsigned long long>(r.bad_pixels),
                100.0 * static_cast<double>(r.bad_pixels) / total, 100.0 * tol.max_bad_fraction, r.max_channel_diff,
                r.mean_abs_diff);
    if (!r.passed && diff_path) {
        if (save_png(diff_path, diff_image(*reference, *actual, tol.pixel_threshold))) {
            std::printf("diff image: %s\n", diff_path);
        }
    }
    return r.passed ? 0 : 1;
}
