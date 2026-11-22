#pragma once
// Game code as feature packages (ADR-0023). A feature is a folder with a feature.json and an
// install() that registers its components, events, systems and scripts. Features talk through
// events and components, never by calling each other; the manifest says what it owns and needs.
//
//     FeatureSet game;
//     game.add("pong_rules");                              // pure script feature: the manifest is enough
//     game.add("pong_audio", [](FeatureContext& f) { f.system<MatchAudio>(Phase::PostUpdate); });
//     if (!game.install(ctx)) ctx.quit();                  // reads assets/features/<name>/feature.json
//
// feature.json:
//   {"format": "astral.feature", "version": 1, "name": "pong_audio", "description": "...",
//    "depends_on": ["pong_rules"],
//    "components": ["pong::Sounds"], "events": [],
//    "exports": {"headers": ["sounds.hpp"]},               // what other features may #include
//    "scripts": ["features/pong_audio/audio.luau"], "actions": [], "settings": {"volume": 0.8}}

#include <astral/app/app.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

struct FeatureManifest {
    std::string name;
    std::string description;
    std::vector<std::string> depends_on;
    std::vector<std::string> components; ///< Components this feature owns (registered by its install()).
    std::vector<std::string> events;     ///< Events it owns.
    std::vector<std::string> headers;    ///< Headers other features may include.
    std::vector<std::string> scripts;    ///< Luau files (asset paths), loaded after install().
    std::vector<std::string> actions;    ///< .actions files (asset paths).
    Json settings = Json::object();
};

/// Reads a manifest. nullopt with *error (naming the problem) if it is not a valid feature.json.
[[nodiscard]] std::optional<FeatureManifest> parse_feature_manifest(const Json& document, std::string* error = nullptr);

/// What install() works with: the game's Context, narrowed to what a feature may do, and the feature's
/// own settings. Registering something the manifest does not list is an error (the manifest is the API).
class FeatureContext {
public:
    Context& game;
    const FeatureManifest& manifest;

    template <class T> void component() {
        declare(Reflect<T>::name, components_, manifest.components, "components");
        game.types.add<T>();
    }
    template <class T> void event() {
        declare(Reflect<T>::name, events_, manifest.events, "events");
        game.events.add<T>();
    }
    template <class S, class... Args> S& system(Phase phase, Args&&... args) {
        return game.add_system<S>(phase, std::forward<Args>(args)...);
    }
    /// settings[key] as a number / string / bool, or `fallback`.
    [[nodiscard]] double number(std::string_view key, double fallback) const;
    [[nodiscard]] std::string text(std::string_view key, std::string_view fallback) const;
    [[nodiscard]] bool flag(std::string_view key, bool fallback) const;
    /// Problems found while installing (undeclared components, ...).
    [[nodiscard]] const std::vector<std::string>& errors() const noexcept { return errors_; }

private:
    friend class FeatureSet;
    FeatureContext(Context& g, const FeatureManifest& m) : game(g), manifest(m) {}
    void declare(std::string_view name, std::vector<std::string>& seen, const std::vector<std::string>& allowed, const char* what);
    std::vector<std::string> components_, events_, errors_;
};

class FeatureSet {
public:
    using Install = std::function<void(FeatureContext&)>;
    /// A feature whose feature.json is read from `<folder>/<name>/feature.json` in the assets at install().
    void add(std::string name, Install install = {});
    /// A feature with its manifest given in code (tests, tools).
    void add(FeatureManifest manifest, Install install = {});

    /// Installs every feature in dependency order (dependencies first; otherwise in the order added).
    /// Stops with false (logged) on the first problem: unreadable manifest, missing dependency,
    /// dependency cycle, name mismatch, undeclared components, failing script. Nothing is installed
    /// when the manifests or the dependency graph are wrong.
    bool install(Context& ctx, std::string_view folder = "features");

    /// The order install() would use, or the problem ("a -> b -> a" cycles, missing dependencies).
    [[nodiscard]] static std::optional<std::vector<std::size_t>> order(const std::vector<FeatureManifest>& manifests, std::string* error);
    [[nodiscard]] const std::vector<FeatureManifest>& manifests() const noexcept { return manifests_; }

private:
    struct Entry {
        std::string name;
        std::optional<FeatureManifest> manifest;
        Install install;
    };
    std::vector<Entry> entries_;
    std::vector<FeatureManifest> manifests_; ///< Filled by install().
};

} // namespace astral
