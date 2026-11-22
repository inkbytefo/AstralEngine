--!strict
-- Rules of the playground. State lives in components (JumpPad, WindZone); events come from the physics.

-- A body that enters a jump pad's sensor gets thrown upwards.
astral.on("astral::TriggerEntered", function(_, event)
    local trigger: Entity?, other: Entity? = event.trigger, event.other
    if not trigger or not other then
        return -- one of them was destroyed in the meantime
    end
    local pad = trigger:get("playground::JumpPad")
    if pad then
        local v = astral.physics.velocity(other)
        astral.physics.set_velocity(other, v + vector.create(0, pad.speed, 0))
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
