#pragma once
// Private: helpers written in Luau, loaded into the global `astral` table before the sandbox closes.
namespace astral {

inline constexpr const char* script_prelude = R"LUAU(
local astral = astral

local eases = {
    linear = function(t) return t end,
    in_quad = function(t) return t * t end,
    out_quad = function(t) return 1 - (1 - t) * (1 - t) end,
    in_out_quad = function(t) if t < 0.5 then return 2 * t * t end return 1 - (-2 * t + 2) ^ 2 / 2 end,
    in_cubic = function(t) return t * t * t end,
    out_cubic = function(t) return 1 - (1 - t) ^ 3 end,
    in_out_cubic = function(t) if t < 0.5 then return 4 * t * t * t end return 1 - (-2 * t + 2) ^ 3 / 2 end,
    out_back = function(t) local c1, c3 = 1.70158, 2.70158 return 1 + c3 * (t - 1) ^ 3 + c1 * (t - 1) ^ 2 end,
}

function astral.ease(name, t)
    local f = eases[name]
    if not f then
        error("unknown ease \"" .. tostring(name) .. "\" (linear, in_quad, out_quad, in_out_quad, in_cubic, out_cubic, in_out_cubic, out_back)", 2)
    end
    return f(math.max(0, math.min(1, t)))
end

function astral.lerp(a, b, t)
    return a + (b - a) * t
end

-- Calls step(t) every frame with the eased progress 0..1 over `duration` seconds, then options.on_done().
-- Returns a timer id: astral.cancel(id) stops it. The tween's progress lives in this closure: for state that must
-- survive a reload or a save, keep it in a component and step it from a system instead.
function astral.tween(duration, step, options)
    options = options or {}
    local ease = eases[options.ease or "linear"]
    if not ease then
        error("unknown ease \"" .. tostring(options.ease) .. "\"", 2)
    end
    if duration <= 0 then
        step(1)
        if options.on_done then options.on_done() end
        return 0
    end
    local elapsed = 0
    local id = 0
    id = astral.every(0.001, function(ctx)
        elapsed = elapsed + ctx.delta
        local t = math.min(elapsed / duration, 1)
        step(ease(t))
        if t >= 1 then
            astral.cancel(id)
            if options.on_done then options.on_done() end
        end
    end)
    return id
end

-- A state machine: states = { idle = { enter = fn(from), update = fn(dt), exit = fn(to), on = { jump = "air" } } }.
-- store = { get = fn() -> string, set = fn(string) } keeps the current state in a component (reload- and save-safe);
-- without it the machine remembers the state itself.
function astral.state_machine(def)
    local states = def.states
    local current = def.initial
    local store = def.store
    local machine = {}
    local function get()
        if store then return store.get() end
        return current
    end
    local function set(name)
        if store then store.set(name) else current = name end
    end
    function machine.state(_)
        return get()
    end
    function machine.go(_, name)
        local to = states[name]
        if not to then
            error("unknown state \"" .. tostring(name) .. "\"", 2)
        end
        local from_name = get()
        local from = states[from_name]
        if from and from.exit then from.exit(name) end
        set(name)
        if to.enter then to.enter(from_name) end
    end
    function machine.send(self, event)
        local s = states[get()]
        local target = s and s.on and s.on[event]
        if target then
            self:go(target)
            return true
        end
        return false
    end
    function machine.update(_, dt)
        local s = states[get()]
        if s and s.update then s.update(dt) end
    end
    if def.initial and not (store and store.get()) then
        set(def.initial)
        local first = states[def.initial]
        if first and first.enter then first.enter(nil) end
    end
    return machine
end
)LUAU";

} // namespace astral
