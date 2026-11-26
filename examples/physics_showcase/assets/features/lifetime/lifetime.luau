--!strict
-- Rule: time runs out, the entity goes.

astral.system({
    name = "lifetime",
    phase = "Update",
    query = { "lifetime::Lifetime" },
    update = function(ctx, entity, life)
        life.seconds = life.seconds - ctx.delta
        if life.seconds <= 0 then
            entity:destroy()
        end
    end,
})
