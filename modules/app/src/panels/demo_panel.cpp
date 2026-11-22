#include "debug_panel.hpp"

#include <imgui.h>

namespace astral {

namespace {

class DemoPanel final : public DebugPanel {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "ImGui demo"; }

    void draw(Context&, DebugPanelState&, bool& open) override { ImGui::ShowDemoWindow(&open); }
};

} // namespace

std::unique_ptr<DebugPanel> make_demo_panel() { return std::make_unique<DemoPanel>(); }

} // namespace astral
