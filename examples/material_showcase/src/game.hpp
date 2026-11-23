#pragma once

#include <astral/app/app.hpp>

namespace material_showcase {

class ShowcaseGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
    void on_update(astral::Context& ctx) override;
};

} // namespace material_showcase
