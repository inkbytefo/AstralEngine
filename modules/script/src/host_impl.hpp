#pragma once
// Private: the Luau state and what the script API needs. Not installed.

#include <astral/script/script.hpp>

#include <lua.h>
#include <lualib.h>

#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astral {

constexpr int tag_entity = 1;
constexpr int tag_component = 2;

struct EntityBox {
    Entity entity;
};
struct ComponentBox {
    Entity entity;
    std::uint32_t type; ///< Index into TypeRegistry::entries().
};

struct SystemDef {
    std::string name;
    ScriptPhase phase = ScriptPhase::Update;
    std::vector<std::uint32_t> types;   ///< Passed to update(), in order.
    std::vector<ComponentId> required;  ///< types + extra "with" components.
    std::vector<ComponentId> excluded;
    int update_ref = 0; ///< Called per entity; 0 if run_ref is used.
    int run_ref = 0;    ///< Called once per phase.
};

struct ListenerDef {
    std::string event;
    ScriptPhase phase = ScriptPhase::Update;
    int fn_ref = 0;
    std::uint64_t cursor = 0;
};

struct Module {
    std::string name;
    int thread_ref = 0; ///< Keeps the sandbox thread (the module's globals) alive.
    std::vector<SystemDef> systems;
    std::vector<ListenerDef> listeners;
    int behaviour_ref = 0; ///< Table with on_start / on_update / on_fixed_update, or 0.
    bool failed = false;
};

struct Timer {
    std::uint32_t id = 0;
    const Module* owner = nullptr;
    double remaining = 0;
    double interval = 0; ///< 0: once.
    int fn_ref = 0;
};

struct ScriptHost::Impl {
    ScriptEnv env;
    ScriptLimits limits;
    lua_State* L = nullptr;
    int ctx_ref = 0; ///< The ctx table handed to callbacks.
    std::vector<std::unique_ptr<Module>> modules;
    Module* active = nullptr; ///< The module whose code is running (registrations belong to it).
    std::vector<Timer> timers;
    std::uint32_t next_timer = 1;
    std::unordered_set<std::uint64_t> started; ///< Entities whose behaviour had on_start.
    std::vector<std::string> errors;
    std::minstd_rand rng{1};
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();

    Impl(const ScriptEnv& e, const ScriptLimits& l) : env(e), limits(l) {}
    ~Impl();

    [[nodiscard]] Module* find_module(std::string_view name) const;
    void release(Module& m);
    void report(const std::string& message);
    void arm_deadline();
    /// Index of the registered component type, or -1.
    [[nodiscard]] int type_index(std::string_view name) const;
    /// Calls the function on the stack top-nargs-1 ... : pcall with error reporting for `m`.
    bool protected_call(Module* m, int nargs);

    static Impl& of(lua_State* L) { return *static_cast<Impl*>(lua_callbacks(L)->userdata); }
};

// --- conversion (convert.cpp) ----------------------------------------------------------------
/// Pushes a field of the object at `base`.
void push_field(lua_State* L, const FieldInfo& field, const void* base, const World& world);
/// Writes the Lua value at `index` into the field; false + *error if the value does not fit.
bool read_field(lua_State* L, int index, const FieldInfo& field, void* base, std::string* error);
void push_entity(lua_State* L, Entity e);
/// The entity at `index` (userdata), or an invalid one for nil; false for anything else.
bool read_entity(lua_State* L, int index, Entity& out);
/// A Lua table (or value) as JSON. `fields` (may be null) tells how to read vectors and entities.
Json lua_to_json(lua_State* L, int index, const std::vector<FieldInfo>* fields);
/// JSON as a Lua value; with `fields`, members become vectors/entities/enumerator names by their kind.
void json_to_lua(lua_State* L, const Json& j, const std::vector<FieldInfo>* fields, World& world);

// --- the astral table (bindings.cpp) ---------------------------------------------------------
void open_astral(lua_State* L, ScriptHost::Impl& impl);

} // namespace astral
