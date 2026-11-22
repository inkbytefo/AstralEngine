--!strict
-- Rule: everything with a Spin turns around its axis. State lives in the components, never here.

astral.system({
    name = "spin",
    phase = "Update",
    query = { "astral::Transform", "blank::Spin" },
    update = function(ctx, _, transform, spin)
        local turn = astral.quat.axis_angle(spin.axis, spin.speed * ctx.delta)
        transform.rotation = astral.quat.normalize(astral.quat.mul(turn, transform.rotation))
    end,
})
