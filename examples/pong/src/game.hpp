#pragma once
// 3D Pong ("Curveball"): a ball in a tunnel, your paddle at the near end, the AI's at the far end.
// A game made of feature packages (ADR-0023):
//   pong_rules        data (types.hpp) + the rules in Luau (assets/features/pong_rules/*.luau)
//   pong_scorelights  C++ system: lights the score pips
//   pong_audio        C++ system: plays the match
// The court, paddles and lights live in assets/*.scene / *.prefab.

#include "features/pong_audio/sounds.hpp"
#include "features/pong_rules/types.hpp"
#include "features/pong_scorelights/lights.hpp"

namespace pong {

/// Installs the three features (components, events, systems, scripts, input actions). Call in
/// on_start, before loading the court scene. False (logged) if a feature cannot be installed.
bool install(astral::Context& ctx);

/// The AI plays your paddle too (demo / capture), or not.
void set_player_ai(astral::World& world, bool on);

/// The game: installs the features, opens the court, draws the HUD and the window title.
/// `demo`: the AI plays your paddle too (captures and --demo).
class PongGame final : public astral::Game {
public:
    explicit PongGame(bool demo) : demo_(demo) {}
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;

private:
    void draw_hud(astral::Context& ctx, const Match& m) const;
    bool demo_;
    int shown_ = -1;
};

/// Current match state (null before the court scene is loaded).
const Match* match(astral::World& world);

} // namespace pong
