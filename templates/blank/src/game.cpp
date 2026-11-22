#include "game.hpp"

#include "features/spin/spin.hpp"

using namespace astral;

namespace blank {

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("spin", install_spin); // reads assets/features/spin/feature.json, then runs install_spin
    return game.install(ctx);
}

void BlankGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    ctx.scenes().load_scene("main.scene");
}

} // namespace blank
