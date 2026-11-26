#pragma once
// physics_showcase: one world, four stations around a lake. The game installs its features, builds the stations and
// draws the HUD; the rules live in the features (C++ systems and Luau).

#include <astral/astral.hpp>

namespace physics_showcase {

/// Entities the tests (and the HUD) look at.
struct Handles {
    astral::Entity player{};
    astral::Entity camera{};
    // Joints station
    astral::Entity arm_body{}, limb_body{}, slider_body{}, fragile_joint{};
    // Surfaces station
    astral::Entity ice_puck{}, grip_puck{}, rubber_ball{}, wheel{}, scanner{}, scanner_gate{};
};

bool install_features(astral::Context& ctx);

class ShowcaseGame final : public astral::Game {
public:
    /// `overlays`: physics shapes, joints and fields over the picture.
    explicit ShowcaseGame(bool overlays = false) : overlays_(overlays) {}
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;

    Handles handles;

private:
    bool overlays_;
};

} // namespace physics_showcase
