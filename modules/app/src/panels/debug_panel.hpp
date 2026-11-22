#pragma once
// Internal to Astral::App: interface and registry for Debug UI panels.

#include <astral/world/world.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace astral {

class Context;
class MaterialLibrary;
class World;

/// Shared state passed to every DebugPanel::draw() call.
struct DebugPanelState {
    MaterialLibrary* materials = nullptr;
    Entity selected{};
};

/// Interface for a window in the F1 debug UI.
class DebugPanel {
public:
    virtual ~DebugPanel() = default;
    DebugPanel(const DebugPanel&) = delete;
    DebugPanel& operator=(const DebugPanel&) = delete;

    /// Display title (used in the Windows menu and as the panel's unique key).
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    /// Whether the panel starts open when the debug UI first appears.
    [[nodiscard]] virtual bool default_open() const noexcept { return false; }
    /// Draws the panel. Setting `open = false` hides the panel until reopened from the Windows menu.
    virtual void draw(Context& ctx, DebugPanelState& state, bool& open) = 0;

protected:
    DebugPanel() = default;
};

/// Owns the registered DebugPanels and tracks their open/closed state in registration order.
class DebugPanelRegistry {
public:
    struct Entry {
        std::string name;
        std::unique_ptr<DebugPanel> panel;
        bool open = false;
    };

    /// Registers a panel (replaces any existing panel with the same name). Null is ignored.
    DebugPanel* add(std::unique_ptr<DebugPanel> panel);

    template <class P, class... Args> P& add(Args&&... args) {
        auto panel = std::make_unique<P>(std::forward<Args>(args)...);
        P& ref = *panel;
        add(std::move(panel));
        return ref;
    }

    /// Removes the panel with `name`. Returns true if one was removed.
    bool remove(std::string_view name);

    [[nodiscard]] DebugPanel* find(std::string_view name) noexcept;
    [[nodiscard]] const DebugPanel* find(std::string_view name) const noexcept;
    [[nodiscard]] bool is_open(std::string_view name) const noexcept;
    bool set_open(std::string_view name, bool open) noexcept;

    [[nodiscard]] std::span<Entry> entries() noexcept { return entries_; }
    [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    /// Draws every currently open panel in registration order.
    void draw_open(Context& ctx, DebugPanelState& state);

private:
    std::vector<Entry> entries_;
};

/// Display label for an entity in Scene and Inspector panels.
[[nodiscard]] std::string entity_label(World& world, Entity e);

/// Registers the engine's built-in panels in menu order.
void register_builtin_panels(DebugPanelRegistry& registry);

// Built-in panel factories (implemented one per file in modules/app/src/panels/).
[[nodiscard]] std::unique_ptr<DebugPanel> make_performance_panel();
[[nodiscard]] std::unique_ptr<DebugPanel> make_scene_panel();
[[nodiscard]] std::unique_ptr<DebugPanel> make_inspector_panel();
[[nodiscard]] std::unique_ptr<DebugPanel> make_render_panel();
[[nodiscard]] std::unique_ptr<DebugPanel> make_world_panel();
[[nodiscard]] std::unique_ptr<DebugPanel> make_materials_panel();
[[nodiscard]] std::unique_ptr<DebugPanel> make_demo_panel();

} // namespace astral
