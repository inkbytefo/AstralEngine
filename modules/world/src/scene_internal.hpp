#pragma once
// Private helpers shared between scene.cpp and prefab.cpp.

#include <astral/world/scene.hpp>
#include <astral/world/transform.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace astral::scene_detail {

inline constexpr std::string_view scene_format = "astral.scene";
inline constexpr int scene_version = 1;
inline constexpr std::string_view parent_name = Reflect<Parent>::name;

/// Internal: on an instance whose parts were spawned, the template they came from. `from` is null
/// (and `failed_path` set) when the prefab could not be built: the error was reported once.
struct PrefabSpawned {
    std::shared_ptr<const PrefabTemplate> from;
    std::string failed_path;
};

struct PlannedPart {
    std::string path; ///< Relative to the instance ("" = its root).
    Json components;  ///< References already placed.
};

/// Reference in -> reference out; nullopt: names nothing.
using RefMap = std::function<std::optional<Json>(const Json&)>;
/// Template path -> placed reference.
using Place = std::function<Json(const std::string&)>;

inline void warn(std::vector<std::string>* errors, std::string message) {
    if (errors) errors->push_back(std::move(message));
}

inline std::uint64_t entity_key(Entity e) { return (std::uint64_t{e.generation} << 32) | e.index; }
inline std::string key_of(std::uint32_t id) { return std::to_string(id); }

std::string normalize_path(std::string_view path);
std::optional<std::uint32_t> record_id(const Json& j);
std::optional<std::pair<std::uint32_t, std::string>> parse_ref(const Json& j);
Json placed_ref(std::uint32_t id, const std::string& rest);
std::string ref_key(const Json& j);
std::string raw_key(Entity e);
std::optional<Entity> parse_raw(std::string_view key);
bool check_scene_format(const Json& doc, std::string* error);

Json remap(const TypeRegistry::Entry& t, const Json& value, const RefMap& map, std::vector<std::string>* errors, std::string label);
std::vector<PlannedPart> plan_instance(const TypeRegistry& types, const PrefabTemplate& tpl, const Json* record,
                                       const Place& place, const RefMap& file_map, std::vector<std::string>* errors,
                                       const std::string& label);
std::vector<Entity> parts_of(World& world, Entity instance);
Json compute_overrides(World& world, const TypeRegistry& types, Entity root, const PrefabTemplate& tpl, const Json& root_ref,
                       const std::function<Json(Entity)>& outer);

/// Creates entities first and loads components last, so references resolve in any order.
struct Spawner {
    World& world;
    const TypeRegistry& types;
    std::vector<std::string>* errors;
    std::unordered_map<std::string, Entity> refs;
    struct Pending {
        Entity entity;
        Json components;
        bool keep_existing; ///< Instance roots: components the entity has win.
        std::string label;
    };
    std::vector<Pending> pending;
    std::unordered_set<const PrefabTemplate*> warned;

    bool resolve(const Json& j, Entity& out) const;
    void finish();
    void spawn(Entity root, const std::shared_ptr<const PrefabTemplate>& tpl, const Json* record, const Json& root_ref,
               const RefMap& file_map, const std::string& label);
};

} // namespace astral::scene_detail
