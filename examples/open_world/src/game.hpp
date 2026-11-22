#pragma once
// The game: opens the map, installs its features, puts the camera in. No rules live here.

#include <astral/astral.hpp>

namespace open_world {

bool install_features(astral::Context& ctx);

/// `capturing`: the camera flies straight ahead by itself and every cell in range is loaded each frame (image tests).
class OpenWorldGame final : public astral::Game {
public:
    OpenWorldGame(bool capturing, bool night) : capturing_(capturing), night_(night) {}
    void on_start(astral::Context& ctx) override;

private:
    bool capturing_;
    bool night_;
};

} // namespace open_world
