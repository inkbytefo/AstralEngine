#pragma once
// The standard main() of an Astral game: command line, window or one offscreen capture, Esc to quit.
// Every game made from templates/blank uses it, so every game has the same flags:
//
//   game                                   play in a window
//   game --capture out.ppm [--frames N] [--width W] [--height H]   render N frames offscreen, write one image
//   game --script-api <folder>             also write astral.d.luau and API.md (the script API)
//   game <anything else>                   left to the game: options.has("--night"), options.value("--view")
//
//     int main(int argc, char** argv) {
//         const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
//         MyGame game(options);
//         return astral::run_game(options, {.title = "My game", .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK}, game);
//     }

#include <astral/app/app.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

struct ShellOptions {
    std::string capture;       ///< Output .ppm path; empty = play in a window.
    std::uint32_t frames = 0;  ///< Frames to run when capturing; 0 = the game's default (run_game's argument).
    std::uint32_t width = 0;   ///< 0 = AppDesc's.
    std::uint32_t height = 0;
    std::string script_api;    ///< Folder for astral.d.luau + API.md; empty = not written.
    std::vector<std::string> args; ///< Every other argument, in order (flags and positional values).

    [[nodiscard]] bool capturing() const noexcept { return !capture.empty(); }
    /// Was `--flag` given?
    [[nodiscard]] bool has(std::string_view flag) const noexcept;
    /// The argument after `--name` (not starting with "--"), or `fallback`.
    [[nodiscard]] std::string value(std::string_view name, std::string_view fallback = {}) const;
    /// Arguments that are neither flags nor the value of a flag known to take one in `value_flags`.
    [[nodiscard]] std::vector<std::string> positional(const std::vector<std::string>& value_flags = {}) const;
};

[[nodiscard]] ShellOptions parse_shell_args(int argc, const char* const* argv);

/// Runs `game`. In a window: until it closes (Esc quits). Capturing: `desc` is made offscreen with a fixed
/// 60 Hz step, `options.frames` (or `default_capture_frames`) frames run, the last image is written
/// as .ppm. Returns the process exit code (1 if the capture failed, e.g. no GPU).
int run_game(const ShellOptions& options, AppDesc desc, Game& game, std::uint32_t default_capture_frames = 60);

} // namespace astral
