#pragma once
// Internal to Astral::App: owns ScriptHost, action/storage adapters, input routing into actions, and script hot reload.

#include "../string_hash.hpp"

#include <astral/core/json.hpp>
#include <astral/script/script.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astral {

class AssetManager;
class EventRegistry;
class Input;
class InputActions;
class Logger;
class Profiler;
class ScriptInput;
class ScriptPhysics;
class ScriptStorage;
class TypeRegistry;
class World;
struct Time;

class ScriptBridge {
public:
    ScriptBridge(World& world, AssetManager& assets, TypeRegistry& types, EventRegistry& events,
                 InputActions& actions, const Time& time, Profiler* profiler, Logger& log,
                 ScriptPhysics* physics, std::filesystem::path save_folder, bool hot_reload);
    ~ScriptBridge();
    ScriptBridge(const ScriptBridge&) = delete;
    ScriptBridge& operator=(const ScriptBridge&) = delete;

    [[nodiscard]] ScriptHost* host() noexcept { return scripts_.get(); }

    void route_input(const Input& raw_input, Input& game_input, bool block_keyboard, bool block_mouse);
    void run(ScriptPhase phase);
    void reload_changed();

    std::vector<std::string> load_script(std::string_view path);
    bool load_actions(std::string_view path);
    bool write_script_api(std::string_view folder, const std::map<std::string, std::string>& api_owners) const;

private:
    struct WatchedFile {
        std::uint64_t hash = 0;
        bool is_actions = false;
    };
    struct ReloadCandidate {
        std::string path;
        std::vector<std::uint8_t> bytes;
        bool is_actions = false;
    };

    std::vector<std::string> load_script_bytes(std::string_view path, const std::vector<std::uint8_t>& bytes);
    bool load_actions_bytes(std::string_view path, const std::vector<std::uint8_t>& bytes);
    void record_watch(std::string_view path, std::uint64_t hash, bool is_actions);

    AssetManager& assets_;
    TypeRegistry& types_;
    EventRegistry& events_;
    InputActions& actions_;
    const Time& time_;
    Profiler* profiler_;
    Logger& log_;
    bool hot_reload_;
    std::unique_ptr<ScriptInput> script_input_;
    std::unique_ptr<ScriptStorage> script_storage_;
    std::unique_ptr<ScriptHost> scripts_;
    std::unordered_map<std::string, WatchedFile, StringHash, std::equal_to<>> watched_files_;
    std::vector<std::pair<std::string, Json>> action_layers_;
    std::vector<ReloadCandidate> reload_scratch_;
    double next_watch_ = 0.0;
};

} // namespace astral
