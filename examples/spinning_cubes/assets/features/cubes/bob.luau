--!strict
-- Rule: a Bob moves up and down around its base height. The phase and height are data.

astral.system({
    name = "bob",
    phase = "Update",
    query = { "astral::Transform", "cubes::Bob" },
    update = function(ctx, _, transform, bob)
        local p = transform.position
        transform.position = vector.create(p.x, bob.base_y + math.sin(ctx.time * 2 + bob.phase) * bob.height, p.z)
    end,
})
