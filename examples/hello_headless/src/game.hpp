#pragma once
#include <astral/astral.hpp>

namespace hello_headless {

bool install_features(astral::Context& ctx);

/// A world with one counter, no window, no GPU.
class HeadlessGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace hello_headless
