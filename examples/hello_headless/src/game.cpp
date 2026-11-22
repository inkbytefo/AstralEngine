#include "game.hpp"

#include "features/tally/tally.hpp"

using namespace astral;

namespace hello_headless {

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("tally", tally::install_tally);
    return game.install(ctx);
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
