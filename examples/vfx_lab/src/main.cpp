#include "game.hpp"

#include <astral/app/shell.hpp>

#include <charconv>
#include <cmath>
#include <string_view>

namespace {

float number_or(std::string_view text, float fallback) noexcept {
    if (text.empty()) return fallback;
    float value = 0.0f;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc{} && ptr == text.data() + text.size() && std::isfinite(value) ? value : fallback;
}

} // namespace

int main(int argc, char** argv) {
    const auto options = astral::parse_shell_args(argc, argv);
    const bool rt = !options.has("--raster");
    vfx_lab::VfxLabGame game({
        .hour = number_or(options.value("--hour", "12"), 12.0f),
        .hours_per_second = number_or(options.value("--hours-per-second", "0"), 0.0f),
        .baseline = options.has("--baseline"),
        .profile = options.has("--profile"),
        .aerial = !options.has("--no-aerial"),
        .fog = number_or(options.value("--fog", "0"), 0.0f),
        .smoke = options.has("--smoke"),
        .vfx = options.has("--vfx"),
    });
    return astral::run_game(options,
                            {.title = "Astral VFX Lab",
                             .render = {.ray_tracing = rt,
                                        .path_tracer = options.has("--path-tracer"),
                                        .fog_self_shadows = !options.has("--no-volume-shadows"),
                                        .fog_ddgi = !options.has("--no-fog-ddgi"),
                                        .sky_ibl = !options.has("--no-sky-ibl")},
                             .asset_root = ASTRAL_GAME_ASSETS,
                             .pack = ASTRAL_GAME_PACK},
                            game, 8);
}
