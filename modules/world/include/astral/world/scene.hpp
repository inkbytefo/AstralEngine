#pragma once
// Scenes and prefabs: JSON documents of entities that keep their structure when saved again.
//
// A scene is a list of entity records with stable ids. A prefab is a scene with exactly one root
// entity (no Parent); placing it in a scene (or in another prefab) makes an *instance*: the record
// names the prefab and stores only what differs from it, field by field.
//
//     {"format": "astral.scene", "version": 1, "entities": [
//       {"id": 1, "components": {"astral::Name": {"value": "Floor"}, "astral::Transform": {...}}},
//       {"id": 2, "prefab": "props/lamp.prefab",
//        "components": {"astral::Transform": {"position": [2, 0, 0]}},   // root overrides
//        "parts": {"3": {"components": {"astral::PointLight": {"intensity": 40}}}},
//        "removed": [...], "deleted": ["5"]}]}
//
// - Entity references are a record id (`1`) or a path into an instance (`"2/3"`: entity 3 of the
//   prefab placed as record 2; deeper nesting adds segments). Parenting into a prefab works the same.
// - Overrides are merge patches: objects merge member by member, everything else replaces.
//   "removed" lists root components the instance dropped, "parts" patches (and "removed") per
//   prefab entity, "deleted" prefab entities the instance destroyed (with their children).
// - Prefabs nest (a prefab may contain instances) and may be variants (the root itself is an
//   instance of another prefab). A prefab that includes itself is an error.
//
// At runtime an instance is an entity with PrefabInstance; its parts carry PrefabPart and belong to
// it (destroyed with it, saved as its overrides, never as records of their own). Edit parts freely:
// save_scene diffs them against the prefab. sync_prefabs() spawns instances created by code and
// respawns instances whose prefab file changed, keeping their overrides.

#include <astral/core/json.hpp>
#include <astral/world/serialize.hpp>
#include <astral/world/world.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace astral {

/// A human-readable name (editor lists, find_by_name, prefab parts). Serializable.
struct Name {
    std::string value;
};

/// Component: "this entity is an instance of this prefab". Its parts are spawned by load_scene,
/// instantiate_prefab or sync_prefabs. Components the entity already has win over the prefab's
/// root components (so `world.create(Transform{...}, PrefabInstance{"x.prefab"})` places it).
struct PrefabInstance {
    std::string path;
};

/// The id of an entity's record in the scene it was loaded from or last saved to. save_scene
/// assigns ids to new entities and keeps existing ones, so saved files diff cleanly.
struct SceneId {
    std::uint32_t value = 0;
};

/// Persistent document address of an entity inside a scene or prefab (ADR-0034):
/// record_id alone ("2") for top-level records, or record_id + part_path ("2/5") inside a prefab instance.
struct DocumentEntityId {
    std::uint32_t record_id = 0;
    std::string part_path;

    [[nodiscard]] bool valid() const noexcept { return record_id != 0; }
    [[nodiscard]] std::string to_string() const {
        if (!valid()) return {};
        return part_path.empty() ? std::to_string(record_id) : std::to_string(record_id) + "/" + part_path;
    }
    [[nodiscard]] static std::optional<DocumentEntityId> parse(std::string_view text) {
        if (text.empty()) return std::nullopt;
        const std::size_t slash = text.find('/');
        const std::string_view head = slash == std::string_view::npos ? text : text.substr(0, slash);
        if (head.empty()) return std::nullopt;
        std::uint32_t id = 0;
        for (char c : head) {
            if (c < '0' || c > '9') return std::nullopt;
            id = id * 10u + static_cast<std::uint32_t>(c - '0');
        }
        if (id == 0) return std::nullopt;
        std::string tail = slash == std::string_view::npos ? std::string{} : std::string(text.substr(slash + 1));
        if (slash != std::string_view::npos && tail.empty()) return std::nullopt;
        return DocumentEntityId{id, std::move(tail)};
    }
    friend bool operator==(const DocumentEntityId&, const DocumentEntityId&) = default;
};

/// On an entity spawned from a prefab: its instance and its path inside the prefab ("3", "4/2").
struct PrefabPart {
    Entity instance;
    std::string path;
};

/// A document by path, as the caller's asset system has it. `version` changes when it reloads.
struct SourceDocument {
    const Json* json = nullptr; ///< Null: missing or unreadable (`error` says why).
    std::uint32_t version = 0;
    std::string error{};
};
using DocumentSource = std::function<SourceDocument(std::string_view path)>;

/// A prefab flattened: nested prefabs expanded and their overrides applied. Paths are relative to
/// the prefab ("" is its root); entity references inside `components` are such paths (strings).
struct PrefabTemplate {
    struct Part {
        std::string path;
        Json components; ///< Object: component name -> value (every field present).
    };
    std::string path;
    std::vector<Part> parts; ///< parts[0] is the root.
    std::vector<std::pair<std::string, std::uint32_t>> sources; ///< Every document used, with its version.
    std::vector<std::string> warnings; ///< Non-fatal problems found while building it.
};

/// Builds prefab templates from documents and caches them until a document they use changes.
class PrefabLibrary {
public:
    PrefabLibrary(const TypeRegistry& types, DocumentSource source);

    /// Null if the prefab cannot be built (missing, malformed, includes itself); `error` says why.
    std::shared_ptr<const PrefabTemplate> get(std::string_view path, std::string* error = nullptr);
    /// Drops templates whose documents changed since they were built (call after documents reload).
    /// Returns how many were dropped; instances notice in sync_prefabs and respawn.
    std::size_t refresh();

    [[nodiscard]] const TypeRegistry& types() const noexcept { return types_; }
    [[nodiscard]] SourceDocument document(std::string_view path) const { return source_(path); }

private:
    std::shared_ptr<const PrefabTemplate> build(std::string_view path, std::string* error);

    const TypeRegistry& types_;
    DocumentSource source_;
    std::unordered_map<std::string, std::shared_ptr<const PrefabTemplate>> cache_;
    std::vector<std::string> building_; ///< Cycle detection.
};

/// Spawns the parts of `instance` (which must have PrefabInstance) and adds the prefab's root
/// components it lacks. Done once per instance; sync_prefabs() does it for you every frame.
LoadResult instantiate_prefab(World& world, PrefabLibrary& prefabs, Entity instance);

/// Keeps instances in step: spawns new ones, respawns those whose prefab changed (reloaded file or
/// new path; a reload keeps their overrides), destroys parts of destroyed or removed instances.
void sync_prefabs(World& world, PrefabLibrary& prefabs, std::vector<std::string>* errors = nullptr);

/// Appends the scene's entities (and their instances' parts) to `world`. Entities get SceneId.
/// Refuses (creating nothing) on a wrong format/version; other problems are reported and skipped.
LoadResult load_scene(World& world, PrefabLibrary& prefabs, const Json& document);

/// Every saveable entity (one with a registered component or a PrefabInstance, not excluded, not a
/// prefab part), ordered by SceneId; entities without one get the next free id (written to the world).
[[nodiscard]] Json save_scene(World& world, PrefabLibrary& prefabs);
/// Like save_scene for the given entities only (prefab parts and dead entities are skipped). References
/// to entities outside the selection are written as null.
[[nodiscard]] Json save_entities(World& world, PrefabLibrary& prefabs, std::span<const Entity> entities);

/// First entity named `name` (no particular order among equals); invalid if none.
[[nodiscard]] Entity find_by_name(World& world, std::string_view name);
/// The part of `instance` named `name`; invalid if none.
[[nodiscard]] Entity find_part(World& world, Entity instance, std::string_view name);

} // namespace astral

ASTRAL_REFLECT(astral::Name, value)
ASTRAL_REFLECT(astral::PrefabInstance, path)
