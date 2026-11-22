// hello_headless — the engine without a window or a GPU: the same features and scripts, a fixed time step, so the
// result is the same on every machine. Runs `--frames N` frames (default 180) and prints what the counter says.
#include "game.hpp"
#include "features/tally/tally.hpp"

#include <astral/app/shell.hpp>
#include <astral/core/version.hpp>

#include <cstdio>

int main(int argc, char** argv) {
    const astral::ShellOptions options = astral::parse_shell_args(argc, argv);
    astral::App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .pack = ASTRAL_GAME_PACK, .hot_reload = false});
    hello_headless::HeadlessGame game;
    app.run_frames(game, options.frames ? options.frames : 180);
    const astral::Entity counter = astral::find_by_name(app.world(), "counter");
    const tally::Tally* t = app.world().get<const tally::Tally>(counter);
    if (!t) return 1;
    std::printf("%d frames, %.3f s of game time (AstralEngine %.*s)\n", t->frames, static_cast<double>(t->seconds),
                static_cast<int>(astral::version().size()), astral::version().data());
    return 0;
}
