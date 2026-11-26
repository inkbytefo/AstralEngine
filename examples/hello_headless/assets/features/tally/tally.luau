--!strict
astral.system({
    name = "tally",
    phase = "Update",
    query = { "tally::Tally" },
    update = function(ctx, _, tally)
        if astral.input.pressed("reset") then
            tally.frames = 0
            tally.seconds = 0
            tally.resets = tally.resets + 1
        end
        if astral.input.pressed("pause") then
            tally.paused = not tally.paused
        end
        if tally.paused then
            return
        end
        local step = astral.input.down("boost") and 2 or 1
        if step > 1 then
            tally.boosted_frames = tally.boosted_frames + 1
        end
        tally.frames = tally.frames + step
        tally.seconds = tally.seconds + ctx.delta * step
    end,
})
