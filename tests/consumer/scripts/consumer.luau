--!strict
astral.system({
    name = "consumer_check",
    phase = "Update",
    query = { "astral::Transform" },
    update = function(ctx, _, transform)
        local dt: number = ctx.delta
        transform.position = transform.position * (1 + dt * 0)
    end,
})
