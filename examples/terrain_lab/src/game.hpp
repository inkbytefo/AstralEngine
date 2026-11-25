#pragma once
#include <astral/astral.hpp>

namespace terrain_lab {

class TerrainLabGame final : public astral::Game {
public:
    explicit TerrainLabGame(bool far_view = false) : far_view_(far_view) {}
    void on_start(astral::Context& ctx) override;

private:
    bool far_view_ = false;
};

} // namespace terrain_lab
