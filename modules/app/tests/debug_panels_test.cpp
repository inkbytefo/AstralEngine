// Headless unit tests for DebugPanel, DebugPanelRegistry, and DevTools panel ownership (M13.5 R2).
#include "../src/panels/debug_panel.hpp"
#include "../src/services/dev_tools.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace astral;

namespace {

class RecordingPanel final : public DebugPanel {
public:
    RecordingPanel(std::string title, bool start_open, std::vector<std::string>& log, bool close_on_draw = false)
        : name_(std::move(title)), default_open_(start_open), log_(log), close_on_draw_(close_on_draw) {}

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }
    [[nodiscard]] bool default_open() const noexcept override { return default_open_; }

    void draw(Context&, DebugPanelState& state, bool& open) override {
        log_.push_back(name_);
        last_selected = state.selected;
        if (close_on_draw_) open = false;
    }

    Entity last_selected{};

private:
    std::string name_;
    bool default_open_;
    std::vector<std::string>& log_;
    bool close_on_draw_;
};

} // namespace

TEST_CASE("DevTools registers built-in panels with expected names and default visibility headlessly") {
    World world;
    DevTools tools(world, nullptr, nullptr, false);
    const DebugPanelRegistry& panels = tools.panels();

    const std::vector<std::pair<std::string, bool>> expected{
        {"Performance", true},
        {"Scene", true},
        {"Inspector", true},
        {"Render", false},
        {"World", false},
        {"Materials", false},
        {"ImGui demo", false},
    };
    REQUIRE(panels.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(panels.entries()[i].name == expected[i].first);
        CHECK(panels.entries()[i].open == expected[i].second);
        CHECK(panels.find(expected[i].first) != nullptr);
        CHECK(panels.is_open(expected[i].first) == expected[i].second);
    }

    // Custom panel registration via DevTools internal API.
    std::vector<std::string> log;
    DebugPanel* added = tools.register_panel(std::make_unique<RecordingPanel>("Game stats", true, log));
    REQUIRE(added != nullptr);
    CHECK(panels.size() == expected.size() + 1);
    CHECK(panels.is_open("Game stats"));
}

TEST_CASE("DebugPanelRegistry manages registration, replacement, removal, and open-panel dispatch headlessly") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f});
    Context& ctx = app.context();

    DebugPanelRegistry registry;
    CHECK(registry.empty());
    CHECK(registry.add(nullptr) == nullptr);

    std::vector<std::string> log;
    auto& alpha = registry.add<RecordingPanel>("Alpha", true, log);
    registry.add<RecordingPanel>("Beta", false, log);
    registry.add<RecordingPanel>("Gamma", true, log, true);

    CHECK(registry.size() == 3);
    CHECK(registry.find("Alpha") == &alpha);
    CHECK(registry.find("Missing") == nullptr);
    CHECK(registry.is_open("Alpha"));
    CHECK_FALSE(registry.is_open("Beta"));
    CHECK(registry.is_open("Gamma"));

    // Only open panels draw, in registration order, receiving shared state.
    const Entity hero = ctx.world.create(Name{"hero"});
    DebugPanelState state{.materials = nullptr, .selected = hero};
    registry.draw_open(ctx, state);
    CHECK(log == std::vector<std::string>{"Alpha", "Gamma"});
    CHECK(alpha.last_selected == hero);

    // Gamma closed itself during draw(); toggling Beta open brings it into the next pass.
    CHECK_FALSE(registry.is_open("Gamma"));
    CHECK(registry.set_open("Beta", true));
    CHECK_FALSE(registry.set_open("Missing", true));
    log.clear();
    registry.draw_open(ctx, state);
    CHECK(log == std::vector<std::string>{"Alpha", "Beta"});

    // Replacing a panel with the same name keeps its slot and resets its open state to the new default.
    registry.add<RecordingPanel>("Beta", false, log);
    CHECK(registry.size() == 3);
    CHECK(registry.entries()[1].name == "Beta");
    CHECK_FALSE(registry.is_open("Beta"));

    // Removing panels updates membership cleanly.
    CHECK(registry.remove("Alpha"));
    CHECK_FALSE(registry.remove("Alpha"));
    CHECK(registry.size() == 2);
    CHECK(registry.find("Alpha") == nullptr);
}

TEST_CASE("entity_label formats named, unnamed, and prefab entities") {
    World world;
    const Entity unnamed = world.create();
    const Entity named = world.create(Name{"player"});
    const Entity prefab = world.create(Name{"lamp"}, PrefabInstance{"props/lamp.prefab"});

    CHECK(entity_label(world, unnamed) == "Entity " + std::to_string(unnamed.index));
    CHECK(entity_label(world, named) == "player");
    CHECK(entity_label(world, prefab) == "lamp  [prefab]");
}
