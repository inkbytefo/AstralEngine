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
#include <astral/core/profiler.hpp>
#include <astral/world/event_registry.hpp>
#include <astral/world/serialize.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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

/// What a script may do with physics and fields (a narrow interface: the App implements it over the physics world).
/// Bodies are addressed by the entity that owns them (RigidBody). `layers` is a mask of collision layers.
struct ScriptHit {
    Entity entity{};
    Vec3 position{};
    Vec3 normal{};
    float distance = 0.0f;
    int surface = 0;
};
struct ScriptRayQuery {
    Vec3 origin{};
    Vec3 direction{0, -1, 0};
    float max_distance = 100.0f;
    std::uint32_t layers = 0xFFFFFFFFu;
    Entity ignore{};
};
struct ScriptWater {
    bool inside = false;
    float surface = 0.0f;
    float depth = 0.0f;
    Vec3 flow{};
    float density = 0.0f;
};
struct ScriptCharacterState {
    Vec3 velocity{};
    bool on_ground = false;
};
class ScriptPhysics {
public:
    virtual ~ScriptPhysics() = default;
    [[nodiscard]] virtual std::optional<ScriptHit> raycast(Vec3 origin, Vec3 direction, float max_distance, std::uint32_t layers, Entity ignore) const = 0;
    [[nodiscard]] virtual std::vector<std::optional<ScriptHit>> raycast_batch(std::span<const ScriptRayQuery> queries) const {
        std::vector<std::optional<ScriptHit>> out;
        out.reserve(queries.size());
        for (const ScriptRayQuery& q : queries) out.push_back(raycast(q.origin, q.direction, q.max_distance, q.layers, q.ignore));
        return out;
    }
    [[nodiscard]] virtual std::optional<ScriptHit> spherecast(Vec3 origin, float radius, Vec3 displacement, std::uint32_t layers, Entity ignore) const = 0;
    [[nodiscard]] virtual std::vector<Entity> overlap_sphere(Vec3 center, float radius, std::uint32_t layers) const = 0;
    [[nodiscard]] virtual std::vector<Entity> overlap_box(Vec3 center, Vec3 half_extents, Quat rotation, std::uint32_t layers) const = 0;
    virtual void impulse(Entity body, Vec3 impulse) = 0;
    virtual void force(Entity body, Vec3 force) = 0;
    virtual void set_velocity(Entity body, Vec3 velocity) = 0;
    [[nodiscard]] virtual Vec3 velocity(Entity body) const = 0;
    virtual bool move_character(Entity, Vec3, bool) { return false; }
    [[nodiscard]] virtual std::optional<ScriptCharacterState> character_state(Entity) const { return std::nullopt; }
    virtual void teleport(Entity, Vec3, Quat) {}
    virtual void wake(Entity) {}
    virtual void sleep(Entity) {}
    virtual void set_joint_motor_mode(Entity, std::string_view) {}
    virtual void set_joint_motor_velocity(Entity, float) {}
    virtual void set_joint_motor_position(Entity, float) {}
    virtual void break_joint(Entity) {}
    [[nodiscard]] virtual Vec3 wind(Vec3 position) const = 0;
    [[nodiscard]] virtual ScriptWater water(Vec3 position) const = 0;
};

/// Where saves go (named slots of text). The App stores files; a test can keep them in memory.
class ScriptStorage {
public:
    virtual ~ScriptStorage() = default;
    virtual bool write(std::string_view slot, std::string_view text) = 0;
    [[nodiscard]] virtual std::optional<std::string> read(std::string_view slot) const = 0;
};

/// Everything the host reaches, handed in at construction (no globals). All must outlive the host.
struct ScriptEnv {
    World& world;
    TypeRegistry& types;
    EventRegistry& events;
    Logger& log;
    ScriptInput* input = nullptr; ///< Null: every action reads as idle.
    ScriptStorage* storage = nullptr; ///< Null: astral.save / astral.load report that saving is not available.
    ScriptPhysics* physics = nullptr; ///< Null: astral.physics.* and astral.fields.* report that physics is not available.
    Profiler* profiler = nullptr; ///< Null: per-module profiler scopes are skipped.
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
