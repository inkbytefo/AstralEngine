--!strict
-- Rules: an Orbit circles the origin and bobs; a Tumble turns around its axis. A frozen one stays where it is.
-- All numbers are data.

astral.system({
    name = "orbit",
    phase = "Update",
    query = { "astral::Transform", "orbits::Orbit" },
    update = function(ctx, _, transform, orbit)
        if not orbit.frozen then
            orbit.phase += orbit.speed * ctx.delta
        end
        local a = orbit.phase
        transform.position = vector.create(
            math.cos(a) * orbit.radius,
            orbit.height + math.sin(a * 1.7) * 0.4,
            math.sin(a) * orbit.radius
        )
    end,
})

astral.system({
    name = "tumble",
    phase = "Update",
    query = { "astral::Transform", "orbits::Tumble" },
    update = function(ctx, _, transform, tumble)
        if tumble.frozen then
            return
        end
        local turn = astral.quat.axis_angle(tumble.axis, tumble.speed * ctx.delta)
        transform.rotation = astral.quat.normalize(astral.quat.mul(turn, transform.rotation))
    end,
})
