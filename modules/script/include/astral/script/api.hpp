#pragma once
// The script API as text, generated from what is registered: Luau type definitions (astral.d.luau, for
// luau-lsp and AI agents) and a readable reference (API.md). Components, fields and events are never
// written by hand, so the documents cannot drift from the code.

#include <astral/world/event_registry.hpp>
#include <astral/world/serialize.hpp>

#include <map>
#include <string>

namespace astral {

struct ScriptApi {
    std::string luau_types; ///< astral.d.luau
    std::string markdown;   ///< API.md
};

/// `owners` maps a component or event name to the feature that owns it (shown in API.md).
[[nodiscard]] ScriptApi generate_script_api(const TypeRegistry& types, const EventRegistry& events,
                                            const std::map<std::string, std::string>& owners = {});

} // namespace astral
