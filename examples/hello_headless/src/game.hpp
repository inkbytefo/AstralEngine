#pragma once
#include <astral/astral.hpp>

#include <vector>

namespace hello_headless {

bool install_features(astral::Context& ctx);

/// Deterministic 120-frame input scenario (boost, pause, resume, reset) for AppDesc::input_script.
[[nodiscard]] std::vector<astral::InputScriptEvent> demo_input_script();

/// A world with one counter, no window, no GPU.
class HeadlessGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace hello_headless
