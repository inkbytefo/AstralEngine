#include <astral/script/api.hpp>

#include <algorithm>

namespace astral {

namespace {

std::string ident(std::string_view name) {
    std::string out;
    for (char c : name) out += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
    return out;
}

std::string luau_type(const FieldInfo& f) {
    switch (f.kind) {
    case FieldKind::Bool: return "boolean";
    case FieldKind::Int:
    case FieldKind::UInt:
    case FieldKind::Float: return "number";
    case FieldKind::Vec2: return "Vec2";
    case FieldKind::Vec3: return "vector";
    case FieldKind::Vec4: return "Vec4";
    case FieldKind::Quat: return "Quat";
    case FieldKind::EntityRef: return "Entity?";
    case FieldKind::String: return "string";
    case FieldKind::Enum: {
        std::string out;
        for (const auto& [name, v] : f.enumerators) out += (out.empty() ? "\"" : " | \"") + name + "\"";
        return out.empty() ? "number" : out;
    }
    default: return "any";
    }
}

bool scriptable(const FieldInfo& f) { return f.kind != FieldKind::Other; }

std::string table_type(const std::vector<FieldInfo>& fields, const char* indent) {
    std::string out = "{\n";
    for (const FieldInfo& f : fields) {
        if (scriptable(f)) out += std::string(indent) + "    " + f.name + ": " + luau_type(f) + ",\n";
    }
    return out + indent + "}";
}

const char* kind_text(const FieldInfo& f) {
    switch (f.kind) {
    case FieldKind::Bool: return "bool";
    case FieldKind::Int: return "int";
    case FieldKind::UInt: return "uint";
    case FieldKind::Float: return "number";
    case FieldKind::Vec2: return "Vec2 {x, y}";
    case FieldKind::Vec3: return "vector";
    case FieldKind::Vec4: return "Vec4 {x, y, z, w}";
    case FieldKind::Quat: return "Quat {x, y, z, w}";
    case FieldKind::EntityRef: return "Entity?";
    case FieldKind::String: return "string";
    case FieldKind::Enum: return "enum";
    default: return "(not scriptable)";
    }
}

std::string enum_values(const FieldInfo& f) {
    std::string out;
    for (const auto& [name, v] : f.enumerators) out += (out.empty() ? "" : ", ") + name;
    return out;
}

} // namespace

ScriptApi generate_script_api(const TypeRegistry& types, const EventRegistry& events, const std::map<std::string, std::string>& owners) {
    ScriptApi api;
    std::string& d = api.luau_types;
    d += "-- astral.d.luau: generated from the registered components and events. Do not edit.\n"
         "export type Vec2 = {x: number, y: number}\n"
         "export type Vec4 = {x: number, y: number, z: number, w: number}\n"
         "export type Quat = {x: number, y: number, z: number, w: number}\n"
         "export type Phase = \"PreUpdate\" | \"Update\" | \"FixedUpdate\" | \"PostUpdate\"\n"
         "export type Ctx = {delta: number, fixed_delta: number, time: number}\n\n";

    // Component proxies.
    for (const auto& t : types.entries()) d += "export type C_" + ident(t.name) + " = " + table_type(t.fields, "") + "\n";
    d += "\n";
    for (const auto& e : events.entries()) {
        if (!e.dynamic) d += "export type E_" + ident(e.name) + " = " + table_type(e.fields, "") + "\n";
    }

    // Name -> type maps; Entity and astral.on/send index them by the string literal that is passed in, so a typo in a
    // component or event name, or in a field, is a type error (type functions keyof / index).
    d += "\nexport type Components = {\n";
    for (const auto& t : types.entries()) d += "    [\"" + std::string(t.name) + "\"]: C_" + ident(t.name) + ",\n";
    d += "}\nexport type Events = {\n";
    for (const auto& e : events.entries()) {
        d += "    [\"" + std::string(e.name) + "\"]: " + (e.dynamic ? std::string("{[string]: any}") : "E_" + ident(e.name)) + ",\n";
    }
    d += "}\n\n";
    d += "export type Entity = {\n"
         "    get: <K>(self: Entity, name: K & keyof<Components>) -> index<Components, K>?,\n"
         "    add: <K>(self: Entity, name: K & keyof<Components>, values: {[string]: any}?) -> index<Components, K>,\n"
         "    has: (self: Entity, name: keyof<Components>) -> boolean,\n"
         "    remove: (self: Entity, name: keyof<Components>) -> (),\n"
         "    destroy: (self: Entity) -> (),\n"
         "    alive: (self: Entity) -> boolean,\n"
         "    id: (self: Entity) -> number,\n}\n\n";

    d += "export type SystemDef = {\n"
         "    name: string?,\n    phase: Phase?,\n    query: {string}?,\n    with: {string}?,\n    without: {string}?,\n"
         "    update: ((ctx: Ctx, entity: Entity, ...any) -> ())?,\n"
         "    run: ((ctx: Ctx) -> ())?,\n}\n"
         "export type Behaviour = {\n"
         "    on_start: ((self: Entity, ctx: Ctx) -> ())?,\n    on_update: ((self: Entity, ctx: Ctx) -> ())?,\n"
         "    on_fixed_update: ((self: Entity, ctx: Ctx) -> ())?,\n}\n\n";

    d += "export type StateDef = {\n    enter: ((from: string?) -> ())?,\n    update: ((dt: number) -> ())?,\n    exit: ((to: string) -> ())?,\n    on: {[string]: string}?,\n}\n"
         "export type StateMachineDef = {\n    initial: string?,\n    states: {[string]: StateDef},\n    store: {get: () -> string?, set: (name: string) -> ()}?,\n}\n"
         "export type StateMachine = {\n    state: (self: StateMachine) -> string,\n    go: (self: StateMachine, name: string) -> (),\n"
         "    send: (self: StateMachine, event: string) -> boolean,\n    update: (self: StateMachine, dt: number) -> (),\n}\n\n";
    d += "declare astral: {\n"
         "    system: (def: SystemDef) -> (),\n"
         "    behaviour: (def: Behaviour) -> Behaviour,\n"
         "    on: <K>(event: K & keyof<Events>, fn: (ctx: Ctx, event: index<Events, K>) -> (), phase: Phase?) -> (),\n"
         "    send: <K>(event: K & keyof<Events>, payload: {[string]: any}?) -> (),\n"
         "    define_component: (name: string, fields: {[string]: string | {type: string, default: any?}}) -> (),\n"
         "    define_event: (name: string) -> (),\n"
         "    spawn: (components: {[string]: any}) -> Entity,\n"
         "    find: (name: string) -> Entity?,\n"
         "    query: (components: {string}) -> {{any}},\n"
         "    after: (seconds: number, fn: () -> ()) -> number,\n"
         "    every: (seconds: number, fn: () -> ()) -> number,\n"
         "    cancel: (timer: number) -> (),\n"
         "    random: () -> number,\n"
         "    seed: (seed: number) -> (),\n"
         "    log: (...any) -> (),\n    warn: (...any) -> (),\n    error: (...any) -> (),\n"
         "    save: (slot: string, components: {string}) -> boolean,\n"
         "    load: (slot: string) -> boolean,\n"
         "    has_save: (slot: string) -> boolean,\n"
         "    ease: (name: string, t: number) -> number,\n"
         "    lerp: <T>(a: T, b: T, t: number) -> T,\n"
         "    tween: (duration: number, step: (t: number) -> (), options: {ease: string?, on_done: (() -> ())?}?) -> number,\n"
         "    state_machine: (def: StateMachineDef) -> StateMachine,\n"
         "    quat: {\n"
         "        axis_angle: (axis: vector, radians: number) -> Quat,\n        mul: (a: Quat, b: Quat) -> Quat,\n"
         "        normalize: (q: Quat) -> Quat,\n        rotate: (q: Quat, v: vector) -> vector,\n    },\n"
         "    input: {\n"
         "        down: (action: string) -> boolean,\n        pressed: (action: string) -> boolean,\n        released: (action: string) -> boolean,\n"
         "        value: (action: string) -> number,\n        axis: (action: string) -> vector,\n    },\n}\n";

    // Markdown reference.
    std::string& m = api.markdown;
    m += "# Script API\n\nGenerated from the registered components and events (do not edit). Scripts are Luau files; "
         "`astral.d.luau` has the same information as types for luau-lsp.\n\n"
         "State lives in components, behaviour in systems. A script never keeps game state in its own variables "
         "(it is reloaded while the game runs).\n\n"
         "## Functions\n\n"
         "| Call | What it does |\n|---|---|\n"
         "| `astral.system{name, phase, query, with, without, update}` | per-entity system: `update(ctx, entity, comp1, comp2, ...)` with the components of `query` |\n"
         "| `astral.system{name, phase, run}` | system that runs once per phase: `run(ctx)` |\n"
         "| `astral.behaviour{on_start, on_update, on_fixed_update}` | behaviour of entities with a `astral::Script` component whose `path` is this file |\n"
         "| `astral.on(event, fn, phase?)` | `fn(ctx, payload)` for each event |\n"
         "| `astral.send(event, payload)` | sends an event |\n"
         "| `astral.define_component(name, {field = \"f32\"})` | defines a component (types: f32, i32, u32, bool, vec2, vec3, vec4, quat, entity; `{type, default}` for defaults) |\n"
         "| `astral.define_event(name)` | defines a script-only event |\n"
         "| `astral.spawn{[\"comp\"] = {fields}}`, `astral.find(name)` | create / look up entities |\n"
         "| `astral.query{\"A\", \"B\"}` | `{ {entity, a, b}, ... }`: every entity with all of them, with live component proxies |\n"
         "| `entity:get(name)`, `:has`, `:add(name, fields)`, `:remove`, `:destroy()`, `:alive()` | component access (`get` returns a live proxy: `c.field = v` writes) |\n"
         "| `astral.after(s, fn)`, `astral.every(s, fn)`, `astral.cancel(id)` | timers |\n"
         "| `astral.tween(seconds, step, {ease, on_done})`, `astral.ease(name, t)`, `astral.lerp(a, b, t)` | animate a value: `step(t)` each frame, t 0..1 eased (linear, in_quad, out_quad, in_out_quad, in_cubic, out_cubic, in_out_cubic, out_back); the tween's progress lives in a closure |\n"
         "| `astral.state_machine{initial, states = {name = {enter, update, exit, on = {event = \"next\"}}}, store}` | `m:send(event)`, `m:go(name)`, `m:update(dt)`, `m:state()`; `store = {get, set}` keeps the state in a component |\n"
         "| `astral.save(slot, {components})`, `astral.load(slot)`, `astral.has_save(slot)` | save the listed components of every entity that has them; load destroys those entities and brings the slot's back |\n"
         "| `astral.quat.axis_angle(axis, radians)`, `.mul(a, b)`, `.rotate(q, v)`, `.normalize(q)` | quaternion math on `{x,y,z,w}` tables (`mul(a, b)` applies b first, like C++) |\n"
         "| `astral.random()`, `astral.seed(n)` | deterministic random in [0, 1) |\n"
         "| `astral.input.down/pressed/released/value/axis(action)` | named input actions |\n"
         "| `astral.log / warn / error(...)` | log lines |\n\n"
         "Phases: `PreUpdate`, `Update`, `FixedUpdate` (fixed step, deterministic: put game rules here), `PostUpdate`. "
         "`ctx` has `delta`, `fixed_delta`, `time`.\n\n"
         "Value types: numbers, booleans, strings; `vector` (Luau native, `vector.create(x, y, z)`); `Vec2`/`Vec4`/`Quat` are tables "
         "`{x=, y=, (z=, w=)}`; enums are their names as strings; entity fields are entities or `nil`.\n\n";
    const auto owner_of = [&](std::string_view name) {
        const auto it = owners.find(std::string(name));
        return it == owners.end() ? std::string() : " (feature `" + it->second + "`)";
    };
    m += "## Components\n\n";
    for (const auto& t : types.entries()) {
        m += "### `" + std::string(t.name) + "`" + owner_of(t.name) + (t.dynamic ? " (defined by a script)" : "") + "\n\n";
        if (t.fields.empty()) {
            m += "A tag: no fields.\n\n";
            continue;
        }
        m += "| Field | Type | Notes |\n|---|---|---|\n";
        for (const FieldInfo& f : t.fields) {
            m += "| `" + f.name + "` | " + kind_text(f) + " | " + (f.kind == FieldKind::Enum ? "one of: " + enum_values(f) : "") + " |\n";
        }
        m += "\n";
    }
    m += "## Events\n\n";
    for (const auto& e : events.entries()) {
        m += "### `" + std::string(e.name) + "`" + owner_of(e.name) + (e.dynamic ? " (script-only: the payload is any table)" : "") + "\n\n";
        if (e.dynamic || e.fields.empty()) {
            m += e.dynamic ? "" : "No fields.\n\n";
            continue;
        }
        m += "| Field | Type | Notes |\n|---|---|---|\n";
        for (const FieldInfo& f : e.fields) {
            m += "| `" + f.name + "` | " + kind_text(f) + " | " + (f.kind == FieldKind::Enum ? "one of: " + enum_values(f) : "") + " |\n";
        }
        m += "\n";
    }
    return api;
}

} // namespace astral
