#pragma once
// The game: installs the free camera and builds the scene (every surface is a .material file). No rules live here.

#include <astral/astral.hpp>

namespace shader_showcase {

bool install_features(astral::Context& ctx);

class ShowcaseGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace shader_showcase
