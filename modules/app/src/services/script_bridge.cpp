#include "script_bridge.hpp"

#include "../json_asset.hpp"

#include <astral/app/components.hpp>
#include <astral/app/input_actions.hpp>
#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>
#include <astral/platform/input.hpp>
#include <astral/script/api.hpp>

#include <cctype>
#include <fstream>
#include <iterator>
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
        std::ofstream out(folder_ / (std::string(slot) + ".json"), std::ios::binary);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(out);
    }
    std::optional<std::string> read(std::string_view slot) const override {
        if (!valid(slot)) return std::nullopt;
        std::ifstream in(folder_ / (std::string(slot) + ".json"), std::ios::binary);
        if (!in) return std::nullopt;
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
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
          ScriptEnv{world, types_, events_, log_, script_input_.get(), script_storage_.get(), physics})) {}

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

std::vector<std::string> ScriptBridge::load_script(std::string_view path) {
    std::string error;
    const auto bytes = assets_.read(path, &error);
    if (!bytes) {
        log_.log(LogLevel::Error, "script " + std::string(path) + ": " + error);
        return {std::string(path) + ": " + error};
    }
    watched_files_[std::string(path)] = {content_hash(*bytes), false};
    return scripts_->load(path, std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

bool ScriptBridge::load_actions(std::string_view path) {
    std::string error;
    const auto bytes = assets_.read(path, &error);
    const auto fail = [&](const std::string& why) {
        log_.log(LogLevel::Error, "actions " + std::string(path) + ": " + why);
        return false;
    };
    if (!bytes) return fail(error);
    const auto doc = parse_json_asset(*bytes, &error);
    if (!doc) return fail(error);
    watched_files_[std::string(path)] = {content_hash(*bytes), true};
    return actions_.load(*doc, &error) || fail(error);
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
    std::vector<std::pair<std::string, bool>> changed;
    for (auto& [path, w] : watched_files_) {
        const auto bytes = assets_.read(path);
        if (bytes && content_hash(*bytes) != w.hash) changed.emplace_back(path, w.is_actions);
    }
    for (const auto& [path, is_actions] : changed) {
        if (is_actions) load_actions(path);
        else load_script(path);
        log_.info("reloaded {}", path);
    }
}

} // namespace astral
