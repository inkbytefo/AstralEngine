--!strict
astral.system({
    name = "tally",
    phase = "Update",
    query = { "tally::Tally" },
    update = function(ctx, _, tally)
        tally.frames = tally.frames + 1
        tally.seconds = tally.seconds + ctx.delta
    end,
})
