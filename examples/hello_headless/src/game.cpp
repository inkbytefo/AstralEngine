#include "game.hpp"

#include "features/tally/tally.hpp"

using namespace astral;

namespace hello_headless {

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("tally", tally::install_tally);
    return game.install(ctx);
}

std::vector<InputScriptEvent> demo_input_script() {
    return {
        {.frame = 30, .key = Key::Space, .down = true},
        {.frame = 45, .key = Key::Space, .down = false},
        {.frame = 60, .key = Key::P, .down = true},
        {.frame = 61, .key = Key::P, .down = false},
        {.frame = 90, .key = Key::P, .down = true},
        {.frame = 91, .key = Key::P, .down = false},
        {.frame = 100, .key = Key::R, .down = true},
        {.frame = 101, .key = Key::R, .down = false},
    };
}

void HeadlessGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    ctx.world.create(Name{"counter"}, tally::Tally{});
    ctx.log.info("{} worker threads", ctx.jobs.worker_count());
}

} // namespace hello_headless
