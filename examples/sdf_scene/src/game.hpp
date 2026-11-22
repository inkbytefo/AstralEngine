#pragma once
// The game: installs its features and builds the scene. No rules live here.

#include <astral/astral.hpp>

namespace sdf_scene {

bool install_features(astral::Context& ctx);

class SdfGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace sdf_scene
