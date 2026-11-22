#pragma once
// The game: installs its features and builds the world (terrain, lake, floaters, bridge, pad). No rules live here.

#include <astral/astral.hpp>

namespace physics_playground {

bool install_features(astral::Context& ctx);

/// Height of the terrain at x, z (metres): hills around a basin; the lake fills the basin up to y = 0.
[[nodiscard]] float terrain_height(float x, float z);

class PlaygroundGame final : public astral::Game {
public:
    /// `overlays`: draw the physics shapes and the fields (wind boxes, water boxes) over the picture.
    explicit PlaygroundGame(bool overlays = false) : overlays_(overlays) {}
    void on_start(astral::Context& ctx) override;

private:
    bool overlays_;
};

} // namespace physics_playground
