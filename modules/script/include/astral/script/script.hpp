#pragma once
// Luau scripting (ADR-0017). Scripts are game rules: systems over components, behaviours of single
// entities, event listeners, timers. State lives in components, never in scripts, so a script can be
// reloaded at any time. Everything a script can reach is the `astral` table (see ScriptHost::load).
//
//     ScriptHost scripts({world, types, events, log});
//     scripts.load("rules.luau", R"(
//         astral.system { phase = "Update", query = {"astral::Transform", "game::Spin"},
//             update = function(ctx, e, t, spin) t.rotation = ... end }
//     )");
//     scripts.run(ScriptPhase::Update, {.delta = 1 / 60.0f});

#include <astral/core/log.hpp>
#include <astral/core/math.hpp>
#include <astral/world/event_registry.hpp>
#include <astral/world/serialize.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

/// Component: this entity runs the behaviour that the script `path` registers with astral.behaviour.
struct Script {
    std::string path;
};

/// The same phases as the App's systems.
enum class ScriptPhase : std::uint8_t { PreUpdate, Update, FixedUpdate, PostUpdate };

/// Time of the frame a phase runs in (seen by scripts as ctx.delta, ctx.fixed_delta, ctx.time).
struct ScriptFrame {
    float delta = 0.0f;
    float fixed_delta = 0.0f;
    double elapsed = 0.0;
};

/// What scripts may ask of the input (named actions, not keys). A narrow interface on purpose.
class ScriptInput {
public:
    virtual ~ScriptInput() = default;
    [[nodiscard]] virtual bool down(std::string_view action) const = 0;
    [[nodiscard]] virtual bool pressed(std::string_view action) const = 0;
    [[nodiscard]] virtual bool released(std::string_view action) const = 0;
    /// -1..1 (buttons: 0 or 1).
    [[nodiscard]] virtual float value(std::string_view action) const = 0;
    [[nodiscard]] virtual Vec2 axis(std::string_view action) const = 0;
};

/// Everything the host reaches, handed in at construction (no globals). All must outlive the host.
struct ScriptEnv {
    World& world;
    TypeRegistry& types;
    EventRegistry& events;
    Logger& log;
    ScriptInput* input = nullptr; ///< Null: every action reads as idle.
};

struct ScriptLimits {
    /// A phase (or a module load) that runs longer is stopped with an error (endless loops).
    double budget_ms = 100.0;
};

class ScriptHost {
public:
    explicit ScriptHost(const ScriptEnv& env, const ScriptLimits& limits = {});
    ~ScriptHost();
    ScriptHost(const ScriptHost&) = delete;
    ScriptHost& operator=(const ScriptHost&) = delete;

    /// Runs `source` as the module `name`: its astral.system / astral.on / astral.behaviour calls
    /// register what the module does. Loading a name again replaces the old module (hot reload); if the
    /// new source fails, the old module keeps running. Returns the problems as "name:line: message"
    /// (empty = loaded).
    std::vector<std::string> load(std::string_view name, std::string_view source);
    /// Removes a module with its systems, listeners, timers and behaviour.
    void unload(std::string_view name);
    [[nodiscard]] bool loaded(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> modules() const;
    /// A module whose callback failed is stopped (logged once) until it is loaded again.
    [[nodiscard]] bool failed(std::string_view name) const;

    /// Runs the phase: event listeners, systems, behaviours (Update: on_update; FixedUpdate:
    /// on_fixed_update), timers (Update).
    void run(ScriptPhase phase, const ScriptFrame& frame);

    /// Runtime errors since the last call (they were logged too).
    [[nodiscard]] std::vector<std::string> take_errors();

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace astral

ASTRAL_REFLECT(astral::Script, path)
