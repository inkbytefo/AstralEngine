// vfx_showcase: four stations (courtyard, particles, smoke, atmosphere) showcasing GPU particles, procedural
// volumes, gameplay smoke, atmosphere + sky IBL + froxel fog, and Luau-driven VFX and sound.
//   vfx_showcase                          1..4 stations, Space VFX+sound, X stop, C disperse smoke, T clock, F/G/I toggles
//   vfx_showcase --capture out.ppm --view courtyard|particles|smoke|atmosphere [--frames N] [--width W] [--height H]
//                [--raster] [--path-tracer] [--hour H] [--hours-per-second R] [--fog F] [--no-aerial] [--baseline]
//                [--no-volume-shadows] [--no-fog-ddgi] [--no-sky-ibl] [--profile]
#include "game.hpp"

#include <astral/app/shell.hpp>

#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>

namespace {

std::optional<float> optional_number(std::string_view text) noexcept {
    if (text.empty()) return std::nullopt;
    float value = 0.0f;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec == std::errc{} && ptr == text.data() + text.size() && std::isfinite(value)) return value;
    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    vfx_showcase::ShowcaseGame game({
        .station = vfx_showcase::station_from_name(options.value("--view", "courtyard")),
        .overrides = {.hour = optional_number(options.value("--hour", "")),
                      .hours_per_second = optional_number(options.value("--hours-per-second", "")),
                      .fog = optional_number(options.value("--fog", "")),
                      .baseline = options.has("--baseline"),
                      .aerial = !options.has("--no-aerial")},
        .raster = options.has("--raster"),
        .path_tracer = options.has("--path-tracer"),
        .fog_self_shadows = !options.has("--no-volume-shadows"),
        .fog_ddgi = !options.has("--no-fog-ddgi"),
        .sky_ibl = !options.has("--no-sky-ibl"),
        .profile = options.has("--profile"),
    });
    return astral::run_game(options,
                            {.title = "Astral — VFX showcase",
                             .render = game.settings(),
                             .asset_root = ASTRAL_GAME_ASSETS,
                             .pack = ASTRAL_GAME_PACK},
                            game, 90);
}
