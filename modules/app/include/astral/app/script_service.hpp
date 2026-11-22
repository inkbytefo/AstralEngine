#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace astral {

/// Luau script loading, input actions loading, and script API generation bound to an App's Context.
class ScriptService {
public:
    virtual ~ScriptService() = default;
    ScriptService(const ScriptService&) = delete;
    ScriptService& operator=(const ScriptService&) = delete;

    /// Loads (or reloads) a script file relative to the asset root; it is reloaded when the file
    /// changes (development builds). Returns the problems as "file:line: message" (empty = fine; they are
    /// logged too). A failing reload keeps the previous version running.
    virtual std::vector<std::string> load_script(std::string_view path) = 0;
    /// Writes astral.d.luau (Luau type definitions) and API.md (reference) for everything registered now:
    /// call after the features are installed. False (logged) if a file cannot be written.
    virtual bool write_script_api(std::string_view folder) = 0;
    /// Loads an .actions file (see input_actions.hpp) into `actions`; reloaded when edited. False (logged)
    /// if it cannot be read or is invalid.
    virtual bool load_actions(std::string_view path) = 0;

protected:
    ScriptService() = default;
};

} // namespace astral
