#pragma once
// The game: opens the map, installs its features, puts the camera in. No rules live here.

#include <astral/astral.hpp>

namespace open_world {

bool install_features(astral::Context& ctx);

/// `capturing`: the camera flies straight ahead by itself and every cell in range is loaded each frame (image tests).
class OpenWorldGame final : public astral::Game {
public:
    /// `nudge`: metres added to the start position (z). A stability test: the picture must not depend on it.
    OpenWorldGame(bool capturing, bool night, float nudge = 0.0f) : capturing_(capturing), night_(night), nudge_(nudge) {}
    void on_start(astral::Context& ctx) override;

private:
    bool capturing_;
    bool night_;
    float nudge_;
};

} // namespace open_world
