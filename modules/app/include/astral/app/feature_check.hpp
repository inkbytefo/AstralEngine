#pragma once
// The build-time guard of ADR-0023: a feature's sources may only include other features' exported
// headers when they depend on them, use no internal engine headers, and keep no mutable global state.
// `astral-features check <sources> <manifests>` runs it; it fails the build on a violation.

#include <filesystem>
#include <string>
#include <vector>

namespace astral {

/// Checks every `<sources>/<feature>/` folder against `<manifests>/<feature>/feature.json`.
/// Returns one readable line per violation ("file:line: what is wrong and what to do").
[[nodiscard]] std::vector<std::string> check_features(const std::filesystem::path& sources, const std::filesystem::path& manifests);

} // namespace astral
