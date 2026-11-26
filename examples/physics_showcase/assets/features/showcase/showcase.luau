--!strict
-- Showcase rules that use the newer Luau physics API: torque + angular velocity, and raycast options.

-- The wheel: measure its spin around the axis, push with torque while it is slower than the target.
astral.system({
    name = "spinner",
    phase = "FixedUpdate",
    query = { "showcase::Spinner" },
    update = function(_, entity, spinner)
        local w = astral.physics.angular_velocity(entity)
        spinner.speed = vector.dot(w, spinner.axis)
        if spinner.speed < spinner.target_speed then
            astral.physics.torque(entity, spinner.axis * spinner.torque)
        end
    end,
})

-- The scanner beam: its own body must not stop it (ignore), and it should see sensor volumes (sensors = true).
astral.system({
    name = "scanner",
    phase = "FixedUpdate",
    query = { "astral::Transform", "showcase::Scanner" },
    update = function(_, entity, transform, scanner)
        local hit = astral.physics.raycast(transform.position, scanner.direction, scanner.reach,
            { ignore = { entity }, sensors = true })
        if hit then
            scanner.hit = hit.entity
            scanner.distance = hit.distance
            scanner.hits += 1
        else
            scanner.hit = nil
            scanner.distance = 0
        end
    end,
})
