#pragma once
// The game: installs its features and opens the first scene. No rules live here.

#include <astral/astral.hpp>

namespace blank {

/// Installs every feature (components, events, systems, scripts, input actions). Add new features here, one line each.
/// False (logged) if one cannot be installed.
bool install_features(astral::Context& ctx);

class BlankGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace blank
