#pragma once
#include <astral/astral.hpp>

namespace material_workflow {

class WorkflowGame final : public astral::Game {
public:
    void on_start(astral::Context& ctx) override;
};

} // namespace material_workflow
