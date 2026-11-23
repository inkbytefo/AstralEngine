#pragma once
// Shared, versioned, discoverable document commands for both human users and AI agents (ADR-0035).

#include <astral/core/json.hpp>
#include <astral/editor/document.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/serialize.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astral::editor {

inline constexpr std::uint32_t kEditorCommandSchemaVersion = 1;

enum class CommandKind : std::uint8_t {
    CreateEntity,
    DeleteEntity,
    AddComponent,
    RemoveComponent,
    SetComponentField,
    ReparentEntity,
    InstantiatePrefab,
    SetPrefabOverride,
    SavePrefab,
};

[[nodiscard]] std::string_view command_kind_name(CommandKind kind) noexcept;
[[nodiscard]] std::optional<CommandKind> command_kind_from_name(std::string_view name) noexcept;

/// One structured, schema-validated document mutation command shared by Inspector UI and AI agents.
struct EditorCommand {
    std::uint32_t schema_version = kEditorCommandSchemaVersion;
    CommandKind kind = CommandKind::SetComponentField;
    DocumentEntityId target{};
    DocumentEntityId parent{};         ///< Used by ReparentEntity (invalid = clear parent).
    std::string component;             ///< Component type name (e.g. "astral::Transform").
    std::string field;                 ///< Field name inside `component` (for SetComponentField/SetPrefabOverride).
    std::string asset_path;            ///< Prefab path (relative to asset root) for InstantiatePrefab/SavePrefab.
    Json value = nullptr;              ///< Component/field JSON payload or initial components map.
};

/// Serializes / parses an EditorCommand to/from JSON (for external AI agents and audit logs).
[[nodiscard]] Json command_to_json(const EditorCommand& cmd);
[[nodiscard]] std::optional<EditorCommand> command_from_json(const Json& j, std::string* error = nullptr);

/// Produces the discoverable JSON schema describing all EditorCommands and registered components/fields.
[[nodiscard]] Json build_command_schema(const TypeRegistry& types);

/// Validates `cmd` against `doc` and `types`. Returns empty string on success, else a diagnostic message.
[[nodiscard]] std::string validate_command(const EditorDocument& doc, const TypeRegistry& types, const EditorCommand& cmd);

/// Applies a validated command to `doc` (and optionally writes a new prefab via `fs` for SavePrefab).
/// Returns empty string on success, else a diagnostic message.
[[nodiscard]] std::string apply_command(EditorDocument& doc, const TypeRegistry& types, const EditorCommand& cmd,
                                        IEditorFileSystem* fs = nullptr,
                                        const std::filesystem::path& asset_root = {});

} // namespace astral::editor
