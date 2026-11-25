#include "script_bridge.hpp"

#include "../json_asset.hpp"

#include <astral/app/components.hpp>
#include <astral/app/input_actions.hpp>
#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>
#include <astral/platform/input.hpp>
#include <astral/script/api.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <utility>

namespace astral {

namespace {

/// What scripts see of the input: the named actions.
class ActionInput final : public ScriptInput {
public:
    explicit ActionInput(const InputActions& a) : actions_(a) {}
    bool down(std::string_view a) const override { return actions_.down(a); }
    bool pressed(std::string_view a) const override { return actions_.pressed(a); }
    bool released(std::string_view a) const override { return actions_.released(a); }
    float value(std::string_view a) const override { return actions_.value(a); }
    Vec2 axis(std::string_view a) const override { return actions_.axis(a); }

private:
    const InputActions& actions_;
};

/// Save slots as files: `<folder>/<slot>.json`. Slot names are letters, digits, '_' and '-' only.
class FileStorage final : public ScriptStorage {
public:
    explicit FileStorage(std::filesystem::path folder) : folder_(std::move(folder)) {}
    bool write(std::string_view slot, std::string_view text) override {
        if (!valid(slot)) return false;
        std::error_code ec;
        std::filesystem::create_directories(folder_, ec);
        const std::filesystem::path target = folder_ / (std::string(slot) + ".json");
        const std::filesystem::path temp = folder_ / (std::string(slot) + ".json.tmp");
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            if (!text.empty()) out.write(text.data(), static_cast<std::streamsize>(text.size()));
            out.flush();
            if (!out) {
                out.close();
                std::filesystem::remove(temp, ec);
                return false;
            }
        }
        std::filesystem::rename(temp, target, ec);
        if (ec) {
            std::filesystem::remove(temp, ec);
            return false;
        }
        return true;
    }
    std::optional<std::string> read(std::string_view slot) const override {
        if (!valid(slot)) return std::nullopt;
        std::ifstream in(folder_ / (std::string(slot) + ".json"), std::ios::binary | std::ios::ate);
        if (!in) return std::nullopt;
        const auto size = in.tellg();
        if (size < 0) return std::nullopt;
        std::string text(static_cast<std::size_t>(size), '\0');
        in.seekg(0);
        if (!text.empty() && !in.read(text.data(), size)) return std::nullopt;
        return text;
    }

private:
    static bool valid(std::string_view slot) {
        if (slot.empty()) return false;
        for (char c : slot) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
        }
        return true;
    }

    std::filesystem::path folder_;
};

std::uint64_t content_hash(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t h = 14695981039346656037ull;
    for (std::uint8_t b : bytes) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

} // namespace

ScriptBridge::ScriptBridge(World& world, AssetManager& assets, TypeRegistry& types, EventRegistry& events,
                           InputActions& actions, const Time& time, Profiler* profiler, Logger& log,
                           ScriptPhysics* physics, std::filesystem::path save_folder, bool hot_reload)
    : assets_(assets),
      types_(types),
      events_(events),
      actions_(actions),
      time_(time),
      profiler_(profiler),
      log_(log),
      hot_reload_(hot_reload),
      script_input_(std::make_unique<ActionInput>(actions_)),
      script_storage_(std::make_unique<FileStorage>(std::move(save_folder))),
      scripts_(std::make_unique<ScriptHost>(
          ScriptEnv{world, types_, events_, log_, script_input_.get(), script_storage_.get(), physics, profiler_})) {}

ScriptBridge::~ScriptBridge() {
    scripts_.reset();
    script_storage_.reset();
    script_input_.reset();
}

void ScriptBridge::route_input(const Input& raw_input, Input& game_input, bool block_keyboard, bool block_mouse) {
    game_input = raw_input;
    if (block_keyboard) game_input.clear_keyboard();
    if (block_mouse) game_input.clear_mouse();
    actions_.update(game_input);
}

void ScriptBridge::run(ScriptPhase phase) {
    ASTRAL_PROFILE_SCOPE(profiler_, "scripts");
    scripts_->run(phase, {.delta = time_.delta, .fixed_delta = time_.fixed_delta, .elapsed = time_.elapsed});
}

void ScriptBridge::record_watch(std::string_view path, std::uint64_t hash, bool is_actions) {
    if (const auto it = watched_files_.find(path); it != watched_files_.end()) {
        it->second = {hash, is_actions};
        return;
    }
    watched_files_.emplace(std::string(path), WatchedFile{hash, is_actions});
}

std::vector<std::string> ScriptBridge::load_script_bytes(std::string_view path, const std::vector<std::uint8_t>& bytes) {
    record_watch(path, content_hash(bytes), false);
    return scripts_->load(path, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

bool ScriptBridge::load_actions_bytes(std::string_view path, const std::vector<std::uint8_t>& bytes) {
    record_watch(path, content_hash(bytes), true);
    std::string error;
    const auto fail = [&](const std::string& why) {
        log_.log(LogLevel::Error, "actions " + std::string(path) + ": " + why);
        return false;
    };
    const auto doc = parse_json_asset(bytes, &error);
    if (!doc) return fail(error);
    InputActions validated;
    if (!validated.load(*doc, &error)) return fail(error);

    const auto existing = std::find_if(action_layers_.begin(), action_layers_.end(),
                                       [&](const auto& entry) { return entry.first == path; });
    const std::optional<Json> previous =
        existing != action_layers_.end() ? std::optional<Json>(existing->second) : std::nullopt;
    if (existing != action_layers_.end()) existing->second = *doc;
    else action_layers_.emplace_back(std::string(path), *doc);

    Json merged = Json::object();
    Json merged_actions = Json::object();
    for (const auto& [layer_path, layer_doc] : action_layers_) {
        (void)layer_path;
        if (const Json* acts = layer_doc.find("actions"); acts && acts->is_object()) {
            for (const auto& [name, spec] : acts->as_object()) merged_actions[name] = spec;
        }
    }
    merged["actions"] = std::move(merged_actions);
    if (!actions_.load(merged, &error)) {
        if (previous) {
            for (auto& entry : action_layers_) {
                if (entry.first == path) entry.second = *previous;
            }
        } else {
            action_layers_.pop_back();
        }
        return fail(error);
    }
    return true;
}

std::vector<std::string> ScriptBridge::load_script(std::string_view path) {
    std::string error;
    const auto bytes = assets_.read(path, &error);
    if (!bytes) {
        log_.log(LogLevel::Error, "script " + std::string(path) + ": " + error);
        return {std::string(path) + ": " + error};
    }
    return load_script_bytes(path, *bytes);
}

bool ScriptBridge::load_actions(std::string_view path) {
    std::string error;
    const auto bytes = assets_.read(path, &error);
    if (!bytes) {
        log_.log(LogLevel::Error, "actions " + std::string(path) + ": " + error);
        return false;
    }
    return load_actions_bytes(path, *bytes);
}

bool ScriptBridge::write_script_api(std::string_view folder, const std::map<std::string, std::string>& api_owners) const {
    const ScriptApi api = generate_script_api(types_, events_, api_owners);
    std::error_code ec;
    const std::filesystem::path dir{std::string(folder)};
    std::filesystem::create_directories(dir, ec);
    for (const auto& [name, text] :
         {std::pair<const char*, const std::string&>{"astral.d.luau", api.luau_types}, {"API.md", api.markdown}}) {
        std::ofstream out(dir / name, std::ios::binary);
        out << text;
        if (!out) {
            log_.log(LogLevel::Error, "script api: cannot write " + (dir / name).string());
            return false;
        }
    }
    return true;
}

void ScriptBridge::reload_changed() {
    if (!hot_reload_ || watched_files_.empty() || time_.elapsed < next_watch_) return;
    next_watch_ = time_.elapsed + 0.5;
    reload_scratch_.clear();
    for (const auto& [path, w] : watched_files_) {
        auto bytes = assets_.read(path);
        if (bytes && content_hash(*bytes) != w.hash) {
            reload_scratch_.push_back({path, std::move(*bytes), w.is_actions});
        }
    }
    for (const auto& candidate : reload_scratch_) {
        const bool ok = candidate.is_actions ? load_actions_bytes(candidate.path, candidate.bytes)
                                             : load_script_bytes(candidate.path, candidate.bytes).empty();
        if (ok) log_.info("reloaded {}", candidate.path);
    }
}

} // namespace astral
