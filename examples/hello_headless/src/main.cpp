// hello_headless — the engine without a window or a GPU: the same features and scripts, a fixed time step, so the
// result is the same on every machine. Runs `--frames N` frames (default 180) and prints what the counter says.
// Pass `--scenario` to run the built-in AppDesc::input_script scenario (default 120 frames).
#include "features/tally/tally.hpp"
#include "game.hpp"

#include <astral/app/shell.hpp>
#include <astral/core/version.hpp>

#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    const bool scenario = options.has("--scenario");
    const std::vector<astral::InputScriptEvent> script = scenario ? hello_headless::demo_input_script() : std::vector<astral::InputScriptEvent>{};
    astral::App app({.headless = true,
                     .fixed_timestep = 1.0f / 60.0f,
                     .asset_root = ASTRAL_GAME_ASSETS,
                     .pack = ASTRAL_GAME_PACK,
                     .hot_reload = false,
                     .input_script = script});
    hello_headless::HeadlessGame game;
    const std::uint32_t default_frames = scenario ? 120u : 180u;
    app.run_frames(game, options.script_api_only ? 1 : options.frames ? options.frames : default_frames);
    if (!options.script_api.empty()) app.context().script_service().write_script_api(options.script_api);
    if (options.script_api_only) return 0;
    const astral::Entity counter = astral::find_by_name(app.world(), "counter");
    const tally::Tally* t = app.world().get<const tally::Tally>(counter);
    if (!t) return 1;
    if (scenario) {
        std::printf("%d frames, %.3f s (%d boosted, %d resets, paused=%s, AstralEngine %.*s)\n", t->frames,
                    static_cast<double>(t->seconds), t->boosted_frames, t->resets, t->paused ? "true" : "false",
                    static_cast<int>(astral::version().size()), astral::version().data());
    } else {
        std::printf("%d frames, %.3f s of game time (AstralEngine %.*s)\n", t->frames, static_cast<double>(t->seconds),
                    static_cast<int>(astral::version().size()), astral::version().data());
    }
    return 0;
}
