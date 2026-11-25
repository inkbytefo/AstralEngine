#pragma once
#include <astral/astral.hpp>

namespace vfx_lab {

struct VfxLabConfig {
    float hour = 12.0f;
    float hours_per_second = 0.0f;
    bool baseline = false;
    bool profile = false;
    bool aerial = true;
    float fog = 0.0f;
    bool smoke = false;
    bool vfx = false;
};

class VfxLabGame final : public astral::Game {
public:
    explicit VfxLabGame(VfxLabConfig config = {}) : config_(config) {}
    void on_start(astral::Context& ctx) override;
    void on_shutdown(astral::Context& ctx) override;

private:
    VfxLabConfig config_{};
};

} // namespace vfx_lab
