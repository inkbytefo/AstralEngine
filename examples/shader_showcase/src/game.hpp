#pragma once
#include <astral/app/app.hpp>

namespace shader_showcase {

class ShowcaseGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace shader_showcase
