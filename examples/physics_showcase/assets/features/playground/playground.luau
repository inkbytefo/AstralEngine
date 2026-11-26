--!strict
-- Rules of the playground. State lives in components (JumpPad, WindZone, ContactStats); events come from physics.

local function ensure_stats(): Entity
    local existing = astral.find("playground_stats")
    if existing then
        return existing
    end
    return astral.spawn({
        ["astral::Name"] = { value = "playground_stats" },
        ["playground::ContactStats"] = { begin_count = 0, pad_launches = 0 },
    })
end

-- A body that enters a jump pad's sensor gets thrown upwards via queued physics commands.
astral.on("astral::TriggerEntered", function(_, event)
    local trigger: Entity?, other: Entity? = event.trigger, event.other
    if not trigger or not other then
        return -- one of them was destroyed in the meantime
    end
    local pad = trigger:get("playground::JumpPad")
    if pad then
        local v = astral.physics.velocity(other)
        astral.physics.set_velocity(other, v + vector.create(0, pad.speed, 0))
        local stats = ensure_stats():get("playground::ContactStats")
        if stats then
            stats.pad_launches += 1
        end
    end
end, "FixedUpdate")

-- Listen to solid contact events and record contact counts in a reflected component (F1).
astral.on("astral::Contact", function(_, event)
    if event.kind == "Begin" then
        local stats = ensure_stats():get("playground::ContactStats")
        if stats then
            stats.begin_count += 1
        end
    end
end, "FixedUpdate")

-- The global wind turns slowly and gusts (the gust numbers are the WindZone's own data).
astral.system({
    name = "turning_wind",
    phase = "Update",
    query = { "astral::WindZone" },
    update = function(ctx, _, zone)
        if zone.global then
            local a = ctx.time * 0.08
            zone.velocity = vector.create(math.cos(a) * 6, 0, math.sin(a) * 6)
        end
    end,
})
