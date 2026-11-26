#pragma once
// Private to Astral::App: where the SPIR-V cache of a game lives (see astral/shader/spirv_cache.hpp).

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace astral {

/// Reads one environment variable ("" when unset). Injected so tests need no process-wide state.
using EnvLookup = std::function<std::string(std::string_view name)>;

/// The real process environment.
[[nodiscard]] std::string process_env(std::string_view name);

/// Folder of the shader cache for the game called `title`; an empty path means "no cache".
///   ASTRAL_SHADER_CACHE=off (or 0)   no cache
///   ASTRAL_SHADER_CACHE=<folder>     that folder, shared by every game that sets it
///   otherwise                        <user cache folder>/AstralEngine/shader-cache/<title>, where the user cache
///                                    folder is %LOCALAPPDATA%, $XDG_CACHE_HOME, ~/.cache or the temp folder.
/// Entries are content-addressed, so games sharing a folder never see each other's shaders: they only share hits.
[[nodiscard]] std::filesystem::path shader_cache_directory(std::string_view title, const EnvLookup& env);

} // namespace astral
