#include "debug_panel.hpp"

#include <astral/world/scene.hpp>
#include <astral/world/world.hpp>

#include <algorithm>

namespace astral {

DebugPanel* DebugPanelRegistry::add(std::unique_ptr<DebugPanel> panel) {
    if (!panel) return nullptr;
    const std::string_view key = panel->name();
    const bool open = panel->default_open();
    for (Entry& e : entries_) {
        if (e.name == key) {
            e.panel = std::move(panel);
            e.open = open;
            return e.panel.get();
        }
    }
    entries_.push_back(Entry{.name = std::string(key), .panel = std::move(panel), .open = open});
    return entries_.back().panel.get();
}

bool DebugPanelRegistry::remove(std::string_view name) {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.name == name; });
    if (it == entries_.end()) return false;
    entries_.erase(it);
    return true;
}

DebugPanel* DebugPanelRegistry::find(std::string_view name) noexcept {
    for (Entry& e : entries_) {
        if (e.name == name) return e.panel.get();
    }
    return nullptr;
}

const DebugPanel* DebugPanelRegistry::find(std::string_view name) const noexcept {
    for (const Entry& e : entries_) {
        if (e.name == name) return e.panel.get();
    }
    return nullptr;
}

bool DebugPanelRegistry::is_open(std::string_view name) const noexcept {
    for (const Entry& e : entries_) {
        if (e.name == name) return e.open;
    }
    return false;
}

bool DebugPanelRegistry::set_open(std::string_view name, bool open) noexcept {
    for (Entry& e : entries_) {
        if (e.name == name) {
            e.open = open;
            return true;
        }
    }
    return false;
}

void DebugPanelRegistry::draw_open(Context& ctx, DebugPanelState& state) {
    for (Entry& e : entries_) {
        if (e.open && e.panel) e.panel->draw(ctx, state, e.open);
    }
}

std::string entity_label(World& world, Entity e) {
    std::string label;
    if (const Name* n = world.get<const Name>(e); n && !n->value.empty()) label = n->value;
    else label = "Entity " + std::to_string(e.index);
    if (world.has<PrefabInstance>(e)) label += "  [prefab]";
    return label;
}

void register_builtin_panels(DebugPanelRegistry& registry) {
    registry.add(make_performance_panel());
    registry.add(make_scene_panel());
    registry.add(make_inspector_panel());
    registry.add(make_render_panel());
    registry.add(make_world_panel());
    registry.add(make_materials_panel());
    registry.add(make_demo_panel());
}

} // namespace astral
