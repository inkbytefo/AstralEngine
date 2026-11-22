#include "pong.hpp"

#include "features/pong_rules/rules.hpp"

using namespace astral;

namespace pong {

bool install(Context& ctx) {
    FeatureSet game;
    game.add("pong_rules", install_rules);
    game.add("pong_scorelights", install_scorelights);
    game.add("pong_audio", install_audio);
    return game.install(ctx);
}

void set_player_ai(World& world, bool on) {
    world.each<Paddle>([&](Paddle& p) {
        if (p.side == Side::Player) p.ai = on;
    });
}

const Match* match(World& world) {
    const Match* found = nullptr;
    world.each<const Match>([&](const Match& m) {
        if (!found) found = &m;
    });
    return found;
}

} // namespace pong
