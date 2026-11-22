#pragma once
// The game: installs its features and builds the grid. No rules live here.

#include <astral/astral.hpp>

namespace spinning_cubes {

bool install_features(astral::Context& ctx);

class CubesGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace spinning_cubes
