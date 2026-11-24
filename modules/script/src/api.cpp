#include <astral/script/api.hpp>

#include <algorithm>
#include <array>

namespace astral {

namespace {

constexpr ScriptSymbolDesc kScriptSymbols[] = {
    // Entity methods
    {"Entity", "get", "<K>(self: Entity, name: K & keyof<Components>) -> index<Components, K>?",
     "`entity:get(name)`, `:has`, `:add(name, fields)`, `:remove`, `:destroy()`, `:alive()`, `:id()`",
     "component access (`get` returns a live proxy: `c.field = v` writes)"},
    {"Entity", "add", "<K>(self: Entity, name: K & keyof<Components>, values: {[string]: any}?) -> index<Components, K>", "", ""},
    {"Entity", "has", "(self: Entity, name: keyof<Components>) -> boolean", "", ""},
    {"Entity", "remove", "(self: Entity, name: keyof<Components>) -> ()", "", ""},
    {"Entity", "destroy", "(self: Entity) -> ()", "", ""},
    {"Entity", "alive", "(self: Entity) -> boolean", "", ""},
    {"Entity", "id", "(self: Entity) -> number", "", ""},

    // Top-level astral functions
    {"", "system", "(def: SystemDef) -> ()",
     "`astral.system{name, phase, query, with, without, update}` / `{name, phase, run}`",
     "per-entity or per-phase system callback"},
    {"", "behaviour", "(def: Behaviour) -> Behaviour",
     "`astral.behaviour{on_start, on_update, on_fixed_update}`",
     "behaviour of entities with an `astral::Script` component whose `path` is this file"},
    {"", "on", "<K>(event: K & keyof<Events>, fn: (ctx: Ctx, event: index<Events, K>) -> (), phase: Phase?) -> ()",
     "`astral.on(event, fn, phase?)`", "`fn(ctx, payload)` for each event"},
    {"", "send", "<K>(event: K & keyof<Events>, payload: {[string]: any}?) -> ()",
     "`astral.send(event, payload)`", "sends an event"},
    {"", "define_component", "(name: string, fields: {[string]: string | {type: string, default: any?}}) -> ()",
     "`astral.define_component(name, {field = \"f32\"})`",
     "defines a component (types: f32, i32, u32, bool, vec2, vec3, vec4, quat, entity; `{type, default}` for defaults)"},
    {"", "define_event", "(name: string) -> ()", "`astral.define_event(name)`", "defines a script-only event"},
    {"", "spawn", "(components: {[string]: any}) -> Entity",
     "`astral.spawn{[\"comp\"] = {fields}}`, `astral.find(name)`", "create / look up entities"},
    {"", "find", "(name: string) -> Entity?", "", ""},
    {"", "query", "(components: {string}) -> {{any}}",
     "`astral.query{\"A\", \"B\"}`", "`{ {entity, a, b}, ... }`: every entity with all of them, with live component proxies"},
    {"", "each", "(components: {string}, fn: (entity: Entity, ...any) -> ()) -> number",
     "`astral.each({\"A\", \"B\"}, function(entity, a, b) ... end)`",
     "batch iteration over matching entities without allocating result tables"},
    {"", "after", "(seconds: number, fn: () -> ()) -> number",
     "`astral.after(s, fn)`, `astral.every(s, fn)`, `astral.cancel(id)`", "timers"},
    {"", "every", "(seconds: number, fn: () -> ()) -> number", "", ""},
    {"", "cancel", "(timer: number) -> ()", "", ""},
    {"", "random", "() -> number", "`astral.random()`, `astral.seed(n)`", "deterministic random in [0, 1)"},
    {"", "seed", "(seed: number) -> ()", "", ""},
    {"", "log", "(...any) -> ()", "`astral.log / warn / error(...)`", "log lines"},
    {"", "warn", "(...any) -> ()", "", ""},
    {"", "error", "(...any) -> ()", "", ""},
    {"", "save", "(slot: string, components: {string}) -> boolean",
     "`astral.save(slot, {components})`, `astral.load(slot)`, `astral.has_save(slot)`",
     "save the listed components of every entity that has them; load destroys those entities and brings the slot's back"},
    {"", "load", "(slot: string) -> boolean", "", ""},
    {"", "has_save", "(slot: string) -> boolean", "", ""},
    {"", "ease", "(name: string, t: number) -> number",
     "`astral.tween(seconds, step, {ease, on_done})`, `astral.ease(name, t)`, `astral.lerp(a, b, t)`",
     "animate a value with easing (linear, in_quad, out_quad, in_out_quad, in_cubic, out_cubic, in_out_cubic, out_back)"},
    {"", "lerp", "<T>(a: T, b: T, t: number) -> T", "", ""},
    {"", "tween", "(duration: number, step: (t: number) -> (), options: {ease: string?, on_done: (() -> ())?}?) -> number", "", ""},
    {"", "state_machine", "(def: StateMachineDef) -> StateMachine",
     "`astral.state_machine{initial, states = ..., store}`",
     "`m:send(event)`, `m:go(name)`, `m:update(dt)`, `m:state()`; `store = {get, set}` keeps state in a component"},

    // astral.physics
    {"physics", "raycast", "(origin: vector, direction: vector, max_distance: number, options: QueryOptions?) -> Hit?",
     "`astral.physics.raycast(origin, direction, max, {layers, ignore})`, `.raycast_batch(queries)`, `.spherecast(...)`",
     "reads the last completed physics step: `{entity, position, normal, distance, surface}` or nil (`false` on batch miss)"},
    {"physics", "raycast_batch", "(queries: {RayQuery}) -> {Hit | boolean}", "", ""},
    {"physics", "spherecast", "(origin: vector, radius: number, displacement: vector, options: QueryOptions?) -> Hit?", "", ""},
    {"physics", "overlap_sphere", "(center: vector, radius: number, options: QueryOptions?) -> {Entity}",
     "`astral.physics.overlap_sphere(center, radius, opts)`, `.overlap_box(center, half, rotation?, opts)`",
     "list of entities overlapping the shape in the last completed physics step"},
    {"physics", "overlap_box", "(center: vector, half_extents: vector, rotation: Quat?, options: QueryOptions?) -> {Entity}", "", ""},
    {"physics", "impulse", "(entity: Entity, impulse: vector) -> ()",
     "`astral.physics.impulse / force / set_velocity / teleport / wake / sleep`, `.velocity(entity)`",
     "queued commands applied deterministically at the fixed-step boundary after body sync; `velocity` reads completed step"},
    {"physics", "force", "(entity: Entity, force: vector) -> ()", "", ""},
    {"physics", "set_velocity", "(entity: Entity, velocity: vector) -> ()", "", ""},
    {"physics", "velocity", "(entity: Entity) -> vector", "", ""},
    {"physics", "move_character", "(entity: Entity, velocity: vector, jump: boolean?) -> boolean",
     "`astral.physics.move_character(entity, velocity, jump?)`",
     "sets horizontal velocity in m/s until replaced; a jump request is consumed once at the next fixed step"},
    {"physics", "character_state", "(entity: Entity) -> CharacterState?",
     "`astral.physics.character_state(entity)`", "velocity and ground contact from the last completed fixed step"},
    {"physics", "teleport", "(entity: Entity, position: vector, rotation: Quat?) -> ()", "", ""},
    {"physics", "wake", "(entity: Entity) -> ()", "", ""},
    {"physics", "sleep", "(entity: Entity) -> ()", "", ""},
    {"physics", "set_joint_motor_mode", "(joint: Entity, mode: \"Off\" | \"Velocity\" | \"Position\") -> ()",
     "`astral.physics.set_joint_motor_mode / _velocity / _position(joint, v)`, `.break_joint(joint)`",
     "queued joint motor and break commands applied deterministically before the next physics step"},
    {"physics", "set_joint_motor_velocity", "(joint: Entity, velocity: number) -> ()", "", ""},
    {"physics", "set_joint_motor_position", "(joint: Entity, position: number) -> ()", "", ""},
    {"physics", "break_joint", "(joint: Entity) -> ()", "", ""},

    // astral.fields
    {"fields", "wind", "(position: vector) -> vector",
     "`astral.fields.wind(position)`, `astral.fields.water(position)`",
     "the wind there now; `{inside, surface, depth, flow, density}`"},
    {"fields", "water", "(position: vector) -> Water", "", ""},

    // astral.quat
    {"quat", "axis_angle", "(axis: vector, radians: number) -> Quat",
     "`astral.quat.axis_angle(axis, radians)`, `.mul(a, b)`, `.rotate(q, v)`, `.normalize(q)`",
     "quaternion math on `{x,y,z,w}` tables (`mul(a, b)` applies b first, like C++)"},
    {"quat", "mul", "(a: Quat, b: Quat) -> Quat", "", ""},
    {"quat", "normalize", "(q: Quat) -> Quat", "", ""},
    {"quat", "rotate", "(q: Quat, v: vector) -> vector", "", ""},

    // astral.input
    {"input", "down", "(action: string) -> boolean",
     "`astral.input.down/pressed/released/value/axis(action)`", "named input actions"},
    {"input", "pressed", "(action: string) -> boolean", "", ""},
    {"input", "released", "(action: string) -> boolean", "", ""},
    {"input", "value", "(action: string) -> number", "", ""},
    {"input", "axis", "(action: string) -> vector", "", ""},
};

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

void write_table_members(std::string& d, std::string_view table, const char* indent) {
    for (const ScriptSymbolDesc& s : kScriptSymbols) {
        if (s.table != table) continue;
        d += std::string(indent) + std::string(s.name) + ": " + std::string(s.luau_type) + ",\n";
    }
}

void write_luau_component_types(std::string& d, const ApiDiscoverySummary& api) {
    d += "-- astral.d.luau: generated from the registered components, events, and binding catalog. Do not edit.\n"
         "export type Vec2 = {x: number, y: number}\n"
         "export type Vec4 = {x: number, y: number, z: number, w: number}\n"
         "export type Quat = {x: number, y: number, z: number, w: number}\n"
         "export type Phase = \"PreUpdate\" | \"Update\" | \"FixedUpdate\" | \"PostUpdate\"\n"
         "export type Ctx = {delta: number, fixed_delta: number, time: number}\n\n";
    for (const ApiTypeSummary& t : api.components) d += "export type C_" + ident(t.name) + " = " + table_type(t.fields, "") + "\n";
    d += "\n";
    for (const ApiTypeSummary& e : api.events) {
        if (!e.dynamic) d += "export type E_" + ident(e.name) + " = " + table_type(e.fields, "") + "\n";
    }
    d += "\nexport type Components = {\n";
    for (const ApiTypeSummary& t : api.components) d += "    [\"" + t.name + "\"]: C_" + ident(t.name) + ",\n";
    d += "}\nexport type Events = {\n";
    for (const ApiTypeSummary& e : api.events) {
        d += "    [\"" + e.name + "\"]: " + (e.dynamic ? std::string("{[string]: any}") : "E_" + ident(e.name)) + ",\n";
    }
    d += "}\n\n";
}

void write_luau_definitions(std::string& d, const ApiDiscoverySummary& api) {
    write_luau_component_types(d, api);
    d += "export type Entity = {\n";
    write_table_members(d, "Entity", "    ");
    d += "}\n\nexport type SystemDef = {\n"
         "    name: string?,\n    phase: Phase?,\n    query: {string}?,\n    with: {string}?,\n    without: {string}?,\n"
         "    update: ((ctx: Ctx, entity: Entity, ...any) -> ())?,\n"
         "    run: ((ctx: Ctx) -> ())?,\n}\n"
         "export type Behaviour = {\n"
         "    on_start: ((self: Entity, ctx: Ctx) -> ())?,\n    on_update: ((self: Entity, ctx: Ctx) -> ())?,\n"
         "    on_fixed_update: ((self: Entity, ctx: Ctx) -> ())?,\n}\n\n"
         "export type QueryOptions = {layers: (number | {number})?, ignore: Entity?}\n"
         "export type RayQuery = {origin: vector, direction: vector, max_distance: number?, options: QueryOptions?}\n"
         "export type CharacterState = {velocity: vector, on_ground: boolean}\n"
         "export type Hit = {entity: Entity, position: vector, normal: vector, distance: number, surface: number}\n"
         "export type Water = {inside: boolean, surface: number, depth: number, flow: vector, density: number}\n"
         "export type StateDef = {\n    enter: ((from: string?) -> ())?,\n    update: ((dt: number) -> ())?,\n"
         "    exit: ((to: string) -> ())?,\n    on: {[string]: string}?,\n}\n"
         "export type StateMachineDef = {\n    initial: string?,\n    states: {[string]: StateDef},\n"
         "    store: {get: () -> string?, set: (name: string) -> ()}?,\n}\n"
         "export type StateMachine = {\n    state: (self: StateMachine) -> string,\n    go: (self: StateMachine, name: string) -> (),\n"
         "    send: (self: StateMachine, event: string) -> boolean,\n    update: (self: StateMachine, dt: number) -> (),\n}\n\n"
         "declare astral: {\n";
    write_table_members(d, "", "    ");
    for (std::string_view sub : {"physics", "fields", "quat", "input"}) {
        d += "    " + std::string(sub) + ": {\n";
        write_table_members(d, sub, "        ");
        d += "    },\n";
    }
    d += "}\n";
}

void write_markdown_reference(std::string& m, const ApiDiscoverySummary& api) {
    m += "# Script API\n\nGenerated from the registered components, events, and binding catalog (do not edit). Scripts are Luau files; "
         "`astral.d.luau` has the same information as types for luau-lsp.\n\n"
         "State lives in components, behaviour in systems. A script never keeps game state in its own variables "
         "(it is reloaded while the game runs).\n\n"
         "## Functions\n\n| Call | What it does |\n|---|---|\n";
    for (const ScriptSymbolDesc& s : api.callables.symbols) {
        if (!s.markdown_call.empty()) {
            m += "| " + std::string(s.markdown_call) + " | " + std::string(s.summary) + " |\n";
        }
    }
    m += "\nPhases: `PreUpdate`, `Update`, `FixedUpdate` (fixed step, deterministic: put game rules here), `PostUpdate`. "
         "`ctx` has `delta`, `fixed_delta`, `time`.\n\n"
         "Value types: numbers, booleans, strings; `vector` (Luau native, `vector.create(x, y, z)`); `Vec2`/`Vec4`/`Quat` are tables "
         "`{x=, y=, (z=, w=)}`; enums are their names as strings; entity fields are entities or `nil`.\n\n"
         "## Components\n\n";
    for (const ApiTypeSummary& t : api.components) {
        const std::string owner = t.owner.empty() ? std::string() : " (feature `" + t.owner + "`)";
        const std::string ver = t.schema_version > 1 ? " (schema v" + std::to_string(t.schema_version) + ")" : "";
        m += "### `" + t.name + "`" + owner + ver + (t.dynamic ? " (defined by a script)" : "") + "\n\n";
        if (t.name == "astral::RigidBody" || t.name == "astral::CharacterController") {
            m += "Scene setup is applied when the physics object is created. The entity Transform supplies the initial pose. "
                 "Use the physics functions for runtime commands; native handles and simulation state are transient.\n\n";
        }
        if (t.name == "astral::JointMotor") {
            m += "The next fixed step applies motor mode, targets, spring frequency (Hz), damping ratio, "
                 "force limit (N), and torque limit (N·m). A nonpositive limit means unlimited.\n\n";
        }
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
    for (const ApiTypeSummary& e : api.events) {
        const std::string owner = e.owner.empty() ? std::string() : " (feature `" + e.owner + "`)";
        m += "### `" + e.name + "`" + owner + (e.dynamic ? " (script-only: the payload is any table)" : "") + "\n\n";
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
}

} // namespace

const ScriptApiCatalog& script_api_catalog() noexcept {
    static const ScriptApiCatalog catalog{kScriptSymbols};
    return catalog;
}

ApiDiscoverySummary collect_api_discovery(const TypeRegistry& types, const EventRegistry& events,
                                          const std::map<std::string, std::string>& owners) {
    ApiDiscoverySummary out;
    out.callables = script_api_catalog();
    auto owner_of = [&](std::string_view name) {
        const auto it = owners.find(std::string(name));
        return it == owners.end() ? std::string() : it->second;
    };
    out.components.reserve(types.entries().size());
    for (const auto& t : types.entries()) {
        out.components.push_back({std::string(t.name), owner_of(t.name), t.schema_version, t.dynamic, t.fields});
    }
    out.events.reserve(events.entries().size());
    for (const auto& e : events.entries()) {
        out.events.push_back({std::string(e.name), owner_of(e.name), 1, e.dynamic, e.fields});
    }
    return out;
}

ScriptApi generate_script_api(const TypeRegistry& types, const EventRegistry& events,
                              const std::map<std::string, std::string>& owners) {
    const ApiDiscoverySummary summary = collect_api_discovery(types, events, owners);
    ScriptApi api;
    write_luau_definitions(api.luau_types, summary);
    write_markdown_reference(api.markdown, summary);
    return api;
}

} // namespace astral
