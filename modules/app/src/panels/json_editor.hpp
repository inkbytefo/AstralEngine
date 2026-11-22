#pragma once
// Internal to Astral::App: Dear ImGui JSON editor shared by Inspector and Render panels.

#include <astral/core/json.hpp>

namespace astral {

/// Draws an editable Dear ImGui widget tree for a reflected Json value; returns true if modified.
bool edit_json(const char* label, Json& j, int depth = 0);

} // namespace astral
