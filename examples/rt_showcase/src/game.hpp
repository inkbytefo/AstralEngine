#pragma once
#include <astral/astral.hpp>

namespace rt_showcase {

class ShowcaseGame final : public astral::Game {
public:
    explicit ShowcaseGame(int view_index = 0) : view_(view_index) {}
    void on_start(astral::Context& ctx) override;

private:
    int view_;
};

} // namespace rt_showcase
