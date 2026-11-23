#pragma once

#include <astral/astral.hpp>

namespace physics_lab {

class PhysicsLabGame final : public astral::Game {
public:
    explicit PhysicsLabGame(bool show_overlays = false) : show_overlays_(show_overlays) {}

    void on_start(astral::Context& ctx) override;

    astral::Entity arm_body{};
    astral::Entity hinge_joint{};
    astral::Entity limb_body{};
    astral::Entity swing_joint{};
    astral::Entity slider_body{};
    astral::Entity sixdof_joint{};
    astral::Entity ice_puck{};
    astral::Entity grip_puck{};
    astral::Entity rubber_ball{};
    astral::Entity fragile_joint{};

private:
    bool show_overlays_ = false;
};

} // namespace physics_lab
