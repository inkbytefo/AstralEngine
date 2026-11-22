// astral-golden: compares a captured frame with a stored reference image.
//
//   astral-golden <actual.ppm|png> <reference.png>
//       [--mode software|hardware] [--device-line <line>]
//       [--diff out.png] [--threshold N] [--max-bad F]
//       [--min-ssim F] [--strong-threshold N] [--max-strong-bad F] [--update]
//
// Exit 0 on pass (or reference update on a verified software device), 125 when skipped due to
// device class mismatch, 1 on image mismatch or I/O error, 2 on invalid CLI usage.

#include "device_info.hpp"
#include "image_compare.hpp"
#include "image_io.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

using namespace astral::golden;

namespace {

constexpr int exit_skip = 125;

enum class Mode {
    Legacy,
    Software,
    Hardware,
};

struct CliOptions {
    const char* actual_path = nullptr;
    const char* reference_path = nullptr;
    const char* diff_path = nullptr;
    const char* device_line = nullptr;
    Mode mode = Mode::Legacy;
    bool update_flag = false;
    Tolerance sw_tol;
    HardwareTolerance hw_tol;
};

bool env_truthy(const char* name) {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || !value) return false;
    const bool truthy = value[0] != '\0' && std::strcmp(value, "0") != 0;
    std::free(value);
    return truthy;
#else
    const char* value = std::getenv(name);
    return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
#endif
}

std::optional<CliOptions> parse_cli(int argc, char** argv) {
    if (argc < 3) return std::nullopt;
    CliOptions o;
    o.actual_path = argv[1];
    o.reference_path = argv[2];
    for (int i = 3; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : ""; };
        if (!std::strcmp(argv[i], "--diff")) o.diff_path = next();
        else if (!std::strcmp(argv[i], "--device-line")) o.device_line = next();
        else if (!std::strcmp(argv[i], "--threshold")) o.sw_tol.pixel_threshold = std::atoi(next());
        else if (!std::strcmp(argv[i], "--max-bad")) o.sw_tol.max_bad_fraction = std::atof(next());
        else if (!std::strcmp(argv[i], "--min-ssim")) o.hw_tol.min_ssim = std::atof(next());
        else if (!std::strcmp(argv[i], "--strong-threshold")) o.hw_tol.strong_pixel_threshold = std::atoi(next());
        else if (!std::strcmp(argv[i], "--max-strong-bad")) o.hw_tol.max_strong_bad_fraction = std::atof(next());
        else if (!std::strcmp(argv[i], "--update")) o.update_flag = true;
        else if (!std::strcmp(argv[i], "--mode")) {
            const char* m = next();
            if (!std::strcmp(m, "software")) o.mode = Mode::Software;
            else if (!std::strcmp(m, "hardware")) o.mode = Mode::Hardware;
            else return std::nullopt;
        }
    }
    return o;
}

int run_software_compare(const CliOptions& o, const Image& actual, bool allow_update) {
    const bool update = allow_update && (o.update_flag || env_truthy("ASTRAL_UPDATE_GOLDEN"));
    const auto reference = load_image(o.reference_path);
    if (update || !reference) {
        if (!update) {
            std::fprintf(stderr, "no reference %s (run with ASTRAL_UPDATE_GOLDEN=1 to create it)\n", o.reference_path);
            return 1;
        }
        if (!save_png(o.reference_path, actual)) {
            std::fprintf(stderr, "cannot write reference %s\n", o.reference_path);
            return 1;
        }
        std::printf("updated reference %s\n", o.reference_path);
        return 0;
    }

    const CompareResult r = compare(*reference, actual, o.sw_tol);
    if (!r.same_size) {
        std::fprintf(stderr, "size mismatch: reference %ux%u, actual %ux%u\n", reference->width, reference->height,
                     actual.width, actual.height);
        return 1;
    }
    const double total = static_cast<double>(reference->width) * reference->height;
    std::printf("%s: bad pixels %llu (%.3f%%, allowed %.3f%%), max diff %d, mean diff %.3f\n",
                r.passed ? "PASS" : "FAIL", static_cast<unsigned long long>(r.bad_pixels),
                100.0 * static_cast<double>(r.bad_pixels) / total, 100.0 * o.sw_tol.max_bad_fraction,
                r.max_channel_diff, r.mean_abs_diff);
    if (!r.passed && o.diff_path) {
        if (save_png(o.diff_path, diff_image(*reference, actual, o.sw_tol.pixel_threshold))) {
            std::printf("diff image: %s\n", o.diff_path);
        }
    }
    return r.passed ? 0 : 1;
}

int run_hardware_compare(const CliOptions& o, const Image& actual) {
    const auto reference = load_image(o.reference_path);
    if (!reference) {
        std::fprintf(stderr, "no reference %s (references must be created on lavapipe with ASTRAL_UPDATE_GOLDEN=1)\n",
                     o.reference_path);
        return 1;
    }
    const HardwareCompareResult r = compare_hardware(*reference, actual, o.hw_tol);
    if (!r.same_size) {
        std::fprintf(stderr, "size mismatch: reference %ux%u, actual %ux%u\n", reference->width, reference->height,
                     actual.width, actual.height);
        return 1;
    }
    const double total = static_cast<double>(reference->width) * reference->height;
    std::printf("%s: ssim %.4f (min %.4f), strong diff >%d: %llu (%.3f%%, allowed %.3f%%), max diff %d, mean diff %.3f\n",
                r.passed ? "PASS" : "FAIL", r.mean_ssim, o.hw_tol.min_ssim, o.hw_tol.strong_pixel_threshold,
                static_cast<unsigned long long>(r.strong_bad_pixels),
                100.0 * static_cast<double>(r.strong_bad_pixels) / total, 100.0 * o.hw_tol.max_strong_bad_fraction,
                r.max_channel_diff, r.mean_abs_diff);
    if (!r.passed && o.diff_path) {
        if (save_png(o.diff_path, diff_image(*reference, actual, o.hw_tol.strong_pixel_threshold))) {
            std::printf("diff image: %s\n", o.diff_path);
        }
    }
    return r.passed ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    const auto parsed = parse_cli(argc, argv);
    if (!parsed) {
        std::fprintf(stderr, "usage: astral-golden <actual> <reference.png> [--mode software|hardware] "
                             "[--device-line <line>] [--diff out.png] [--threshold N] [--max-bad F] "
                             "[--min-ssim F] [--strong-threshold N] [--max-strong-bad F] [--update]\n");
        return 2;
    }
    const CliOptions& o = *parsed;

    if (o.mode == Mode::Hardware && o.update_flag) {
        std::fprintf(stderr, "error: --update is forbidden in hardware golden mode (references require lavapipe)\n");
        return 2;
    }

    std::optional<CaptureDevice> device;
    if (o.device_line && o.device_line[0] != '\0') {
        device = parse_capture_device(o.device_line);
        if (device) std::printf("device: %s\n", format_device(*device).c_str());
    }

    if (o.mode == Mode::Software) {
        if (!device) {
            std::fprintf(stderr, "error: capture did not report Vulkan device identity\n");
            return 1;
        }
        if (classify_device(*device) != DeviceClass::Lavapipe) {
            std::printf("SKIP: software golden requires lavapipe/llvmpipe (rendered on %s)\n",
                        format_device(*device).c_str());
            return exit_skip;
        }
    } else if (o.mode == Mode::Hardware) {
        if (!device) {
            std::fprintf(stderr, "error: capture did not report Vulkan device identity\n");
            return 1;
        }
        if (classify_device(*device) != DeviceClass::Hardware) {
            std::printf("SKIP: hardware golden requires a non-software GPU (rendered on %s)\n",
                        format_device(*device).c_str());
            return exit_skip;
        }
    }

    const auto actual = load_image(o.actual_path);
    if (!actual) {
        std::fprintf(stderr, "cannot read captured image %s\n", o.actual_path);
        return 1;
    }

    if (o.mode == Mode::Hardware) return run_hardware_compare(o, *actual);
    const bool allow_update = (o.mode == Mode::Legacy) || (device && classify_device(*device) == DeviceClass::Lavapipe);
    return run_software_compare(o, *actual, allow_update);
}
