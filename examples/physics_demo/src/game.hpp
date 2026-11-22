#pragma once
// The game: installs its features and builds the world. No rules live here.

#include <astral/astral.hpp>

namespace physics_demo {

bool install_features(astral::Context& ctx);

class PhysicsGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace physics_demo
