#pragma once
// The script API as text and structured discovery metadata (ADR-0034):
// Reflection (TypeRegistry / EventRegistry) is the single source of data types and fields,
// while ScriptApiCatalog is the single source of callable script bindings. Inspector, API.md,
// astral.d.luau, and runtime binding parity tests all read from these unified surfaces.

#include <astral/world/event_registry.hpp>
#include <astral/world/serialize.hpp>

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

/// Metadata for one callable script binding (under `astral`, a subtable like `physics`, or `Entity`).
struct ScriptSymbolDesc {
    std::string_view table;          ///< "", "physics", "fields", "quat", "input", or "Entity"
    std::string_view name;           ///< Function/method name as exposed in Luau
    std::string_view luau_type;      ///< Luau type signature for astral.d.luau
    std::string_view markdown_call;  ///< Display call in API.md (empty to omit duplicate row)
    std::string_view summary;        ///< One-line description in API.md
};

/// Single authoritative catalog of callable script bindings.
struct ScriptApiCatalog {
    std::span<const ScriptSymbolDesc> symbols;
};

[[nodiscard]] const ScriptApiCatalog& script_api_catalog() noexcept;

/// Unified discovery snapshot shared by Inspector, API docs, script types, and editor tools.
struct ApiTypeSummary {
    std::string name;
    std::string owner;
    std::uint32_t schema_version = 1;
    bool dynamic = false;
    std::vector<FieldInfo> fields;
};

struct ApiDiscoverySummary {
    std::vector<ApiTypeSummary> components;
    std::vector<ApiTypeSummary> events;
    ScriptApiCatalog callables;
};

[[nodiscard]] ApiDiscoverySummary collect_api_discovery(const TypeRegistry& types, const EventRegistry& events,
                                                        const std::map<std::string, std::string>& owners = {});

struct ScriptApi {
    std::string luau_types; ///< astral.d.luau
    std::string markdown;   ///< API.md
};

/// `owners` maps a component or event name to the feature that owns it (shown in API.md).
[[nodiscard]] ScriptApi generate_script_api(const TypeRegistry& types, const EventRegistry& events,
                                            const std::map<std::string, std::string>& owners = {});

} // namespace astral
