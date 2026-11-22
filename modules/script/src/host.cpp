#include "host_impl.hpp"

#include <astral/script/compile.hpp>

#include "prelude.hpp"

#include <luacode.h>

#include <algorithm>
#include <cstdlib>

namespace astral {

using Impl = ScriptHost::Impl;

namespace {

void interrupt(lua_State* L, int gc) {
    if (gc >= 0) return;
    Impl& impl = Impl::of(L);
    if (std::chrono::steady_clock::now() > impl.deadline) {
        impl.deadline = std::chrono::steady_clock::time_point::max(); // report once
        luaL_error(L, "script exceeded its time budget of %g ms (an endless loop?)", impl.limits.budget_ms);
    }
}

std::uint64_t key_of(Entity e) { return (std::uint64_t{e.generation} << 32) | e.index; }

} // namespace

Impl::~Impl() {
    if (L) lua_close(L);
}

Module* Impl::find_module(std::string_view name) const {
    for (const auto& m : modules) {
        if (m->name == name) return m.get();
    }
    return nullptr;
}

void Impl::release(Module& m) {
    for (SystemDef& s : m.systems) {
        if (s.update_ref) lua_unref(L, s.update_ref);
        if (s.run_ref) lua_unref(L, s.run_ref);
    }
    for (ListenerDef& l : m.listeners) lua_unref(L, l.fn_ref);
    if (m.behaviour_ref) lua_unref(L, m.behaviour_ref);
    for (auto it = timers.begin(); it != timers.end();) {
        if (it->owner == &m) {
            lua_unref(L, it->fn_ref);
            it = timers.erase(it);
        } else {
            ++it;
        }
    }
    if (m.thread_ref) lua_unref(L, m.thread_ref);
    m.systems.clear();
    m.listeners.clear();
    m.behaviour_ref = m.thread_ref = 0;
}

void Impl::report(const std::string& message) {
    errors.push_back(message);
    env.log.log(LogLevel::Error, "script: " + message);
}

void Impl::arm_deadline() {
    deadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                       std::chrono::duration<double, std::milli>(limits.budget_ms));
}

int Impl::type_index(std::string_view name) const {
    const TypeRegistry::Entry* e = env.types.find(name);
    return e ? static_cast<int>(e - env.types.entries().data()) : -1;
}

bool Impl::protected_call(Module* m, int nargs) {
    Module* previous = active;
    active = m;
    const int rc = lua_pcall(L, nargs, 0, 0);
    active = previous;
    if (rc == 0) return true;
    const char* text = lua_tostring(L, -1);
    report(text ? text : "unknown script error");
    lua_pop(L, 1);
    if (m) m->failed = true;
    return false;
}

ScriptHost::ScriptHost(const ScriptEnv& env, const ScriptLimits& limits) : impl_(std::make_unique<Impl>(env, limits)) {
    env.types.add<Script>();
    lua_State* L = luaL_newstate();
    impl_->L = L;
    luaL_openlibs(L);
    lua_callbacks(L)->userdata = impl_.get();
    lua_callbacks(L)->interrupt = interrupt;
    open_astral(L, *impl_);
    {   // helpers written in Luau (tween, state machine): run once, before the globals become read-only
        std::size_t size = 0;
        char* bytecode = luau_compile(script_prelude, std::char_traits<char>::length(script_prelude), nullptr, &size);
        if (luau_load(L, "=astral prelude", bytecode, size, 0) != 0 || lua_pcall(L, 0, 0, 0) != 0) {
            env.log.log(LogLevel::Error, std::string("script prelude: ") + (lua_tostring(L, -1) ? lua_tostring(L, -1) : "?"));
            lua_pop(L, 1);
        }
        std::free(bytecode);
    }
    luaL_sandbox(L);
    lua_newtable(L);
    impl_->ctx_ref = lua_ref(L, -1);
    lua_pop(L, 1);
}

ScriptHost::~ScriptHost() = default;

std::vector<std::string> ScriptHost::load(std::string_view name, std::string_view source) {
    Impl& impl = *impl_;
    lua_State* L = impl.L;
    impl.arm_deadline();
    auto module = std::make_unique<Module>();
    module->name = std::string(name);

    lua_State* T = lua_newthread(L);
    module->thread_ref = lua_ref(L, -1);
    lua_pop(L, 1);
    luaL_sandboxthread(T);

    const std::string chunk = "=" + std::string(name);
    int loaded;
    if (is_compiled_script(source)) { // cooked: bytecode after the magic
        const std::string_view code = source.substr(4);
        loaded = luau_load(T, chunk.c_str(), code.data(), code.size(), 0);
    } else {
        std::size_t size = 0;
        char* bytecode = luau_compile(source.data(), source.size(), nullptr, &size);
        loaded = luau_load(T, chunk.c_str(), bytecode, size, 0);
        std::free(bytecode);
    }
    std::string failure;
    if (loaded != 0) {
        failure = lua_tostring(T, -1) ? lua_tostring(T, -1) : "cannot load";
    } else {
        Module* previous = impl.active;
        impl.active = module.get();
        if (lua_pcall(T, 0, 0, 0) != 0) failure = lua_tostring(T, -1) ? lua_tostring(T, -1) : "script error";
        impl.active = previous;
    }
    if (!failure.empty()) {
        impl.release(*module);
        impl.env.log.log(LogLevel::Error, "script: " + failure);
        return {failure};
    }
    for (auto& existing : impl.modules) {
        if (existing->name == module->name) { // hot reload: the new version takes the old one's place
            impl.release(*existing);
            existing = std::move(module);
            return {};
        }
    }
    impl.modules.push_back(std::move(module));
    return {};
}

void ScriptHost::unload(std::string_view name) {
    auto& modules = impl_->modules;
    for (auto it = modules.begin(); it != modules.end(); ++it) {
        if ((*it)->name == name) {
            impl_->release(**it);
            modules.erase(it);
            return;
        }
    }
}

bool ScriptHost::loaded(std::string_view name) const { return impl_->find_module(name) != nullptr; }

bool ScriptHost::failed(std::string_view name) const {
    const Module* m = impl_->find_module(name);
    return m && m->failed;
}

std::vector<std::string> ScriptHost::modules() const {
    std::vector<std::string> out;
    for (const auto& m : impl_->modules) out.push_back(m->name);
    return out;
}

std::vector<std::string> ScriptHost::take_errors() { return std::exchange(impl_->errors, {}); }

namespace {

void run_system(Impl& impl, Module& m, const SystemDef& def) {
    lua_State* L = impl.L;
    World& world = impl.env.world;
    if (def.run_ref) {
        lua_getref(L, def.run_ref);
        lua_getref(L, impl.ctx_ref);
        impl.protected_call(&m, 1);
        return;
    }
    // Collect first: scripts may create and destroy entities while they run.
    std::vector<Entity> entities;
    world.each_raw(def.required, [&](Entity e) {
        for (ComponentId id : def.excluded) {
            if (world.has_raw(e, id)) return;
        }
        entities.push_back(e);
    });
    for (Entity e : entities) {
        if (m.failed) return;
        bool ok = world.alive(e);
        for (std::size_t i = 0; ok && i < def.required.size(); ++i) ok = world.has_raw(e, def.required[i]);
        for (std::size_t i = 0; ok && i < def.excluded.size(); ++i) ok = !world.has_raw(e, def.excluded[i]);
        if (!ok) continue;
        lua_getref(L, def.update_ref);
        lua_getref(L, impl.ctx_ref);
        push_entity(L, e);
        for (std::uint32_t t : def.types) {
            auto* box = static_cast<ComponentBox*>(lua_newuserdatataggedwithmetatable(L, sizeof(ComponentBox), tag_component));
            box->entity = e;
            box->type = t;
        }
        impl.protected_call(&m, 2 + static_cast<int>(def.types.size()));
    }
}

void run_listeners(Impl& impl, Module& m, ScriptPhase phase) {
    lua_State* L = impl.L;
    for (ListenerDef& l : m.listeners) {
        if (l.phase != phase || m.failed) continue;
        const EventRegistry::Entry* entry = impl.env.events.find(l.event);
        if (!entry) continue;
        std::vector<Json> payloads;
        entry->read(impl.env.world, l.cursor, [&](const Json& j) { payloads.push_back(j); });
        for (const Json& payload : payloads) {
            if (m.failed) return;
            lua_getref(L, l.fn_ref);
            lua_getref(L, impl.ctx_ref);
            json_to_lua(L, payload, entry->fields.empty() ? nullptr : &entry->fields, impl.env.world);
            impl.protected_call(&m, 2);
        }
    }
}

void call_behaviour(Impl& impl, Module& m, const char* function, Entity e) {
    lua_State* L = impl.L;
    lua_getref(L, m.behaviour_ref);
    lua_getfield(L, -1, function);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return;
    }
    lua_remove(L, -2);
    push_entity(L, e);
    lua_getref(L, impl.ctx_ref);
    impl.protected_call(&m, 2);
}

void run_behaviours(Impl& impl, ScriptPhase phase) {
    World& world = impl.env.world;
    const ComponentId id = component_id<Script>();
    std::vector<Entity> entities;
    world.each_raw(std::span<const ComponentId>(&id, 1), [&](Entity e) { entities.push_back(e); });
    for (Entity e : entities) {
        const Script* script = world.get<const Script>(e);
        if (!script) continue;
        Module* m = impl.find_module(script->path);
        if (!m || m->failed || !m->behaviour_ref) continue;
        if (phase == ScriptPhase::Update) {
            if (impl.started.insert(key_of(e)).second) call_behaviour(impl, *m, "on_start", e);
            if (!m->failed && world.alive(e)) call_behaviour(impl, *m, "on_update", e);
        } else if (phase == ScriptPhase::FixedUpdate && impl.started.contains(key_of(e))) {
            call_behaviour(impl, *m, "on_fixed_update", e);
        }
    }
    if (phase == ScriptPhase::Update) {
        for (auto it = impl.started.begin(); it != impl.started.end();) {
            const Entity e{static_cast<std::uint32_t>(*it & 0xffffffffu), static_cast<std::uint32_t>(*it >> 32)};
            it = world.alive(e) ? std::next(it) : impl.started.erase(it);
        }
    }
}

void run_timers(Impl& impl, double delta) {
    lua_State* L = impl.L;
    std::vector<std::uint32_t> due;
    for (Timer& t : impl.timers) {
        if (t.owner->failed) continue;
        t.remaining -= delta;
        if (t.remaining <= 0) due.push_back(t.id);
    }
    for (std::uint32_t id : due) {
        auto it = std::find_if(impl.timers.begin(), impl.timers.end(), [id](const Timer& t) { return t.id == id; });
        if (it == impl.timers.end()) continue; // cancelled by an earlier callback
        Module* owner = const_cast<Module*>(it->owner);
        const int fn = it->fn_ref;
        if (it->interval > 0) {
            it->remaining += it->interval;
        }
        const bool once = it->interval <= 0;
        lua_getref(L, fn);
        lua_getref(L, impl.ctx_ref);
        if (once) { // free the entry first: the callback may add timers
            impl.timers.erase(it);
        }
        impl.protected_call(owner, 1);
        if (once) lua_unref(L, fn);
    }
}

} // namespace

void ScriptHost::run(ScriptPhase phase, const ScriptFrame& frame) {
    Impl& impl = *impl_;
    lua_State* L = impl.L;
    impl.arm_deadline();
    lua_getref(L, impl.ctx_ref);
    lua_pushnumber(L, static_cast<double>(frame.delta));
    lua_setfield(L, -2, "delta");
    lua_pushnumber(L, static_cast<double>(frame.fixed_delta));
    lua_setfield(L, -2, "fixed_delta");
    lua_pushnumber(L, frame.elapsed);
    lua_setfield(L, -2, "time");
    lua_pop(L, 1);

    // Index loop: a callback may load modules.
    for (std::size_t i = 0; i < impl.modules.size(); ++i) {
        Module& m = *impl.modules[i];
        if (m.failed) continue;
        run_listeners(impl, m, phase);
        for (std::size_t s = 0; s < m.systems.size() && !m.failed; ++s) {
            if (m.systems[s].phase == phase) run_system(impl, m, m.systems[s]);
        }
    }
    if (phase == ScriptPhase::Update || phase == ScriptPhase::FixedUpdate) run_behaviours(impl, phase);
    if (phase == ScriptPhase::Update) run_timers(impl, static_cast<double>(frame.delta));
    impl.deadline = std::chrono::steady_clock::time_point::max();
}

} // namespace astral
