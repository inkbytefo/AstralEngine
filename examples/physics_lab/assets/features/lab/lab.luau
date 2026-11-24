--!strict
-- P0 Physics Lab rules: drives motorized joints, listens for JointBroken / JointDetached, and runs batched raycasts.

local function get_stats(): Entity
    local existing = astral.find("lab_stats")
    if existing then
        return existing
    end
    return astral.spawn({
        ["astral::Name"] = { value = "lab_stats" },
        ["lab::LabStats"] = { broken_joints = 0, detached_joints = 0, batch_ray_hits = 0, motor_updates = 0 },
    })
end

astral.on("astral::JointBroken", function(_, event)
    if event.joint then
        local s = get_stats():get("lab::LabStats")
        if s then
            s.broken_joints += 1
        end
    end
end, "FixedUpdate")

astral.on("astral::JointDetached", function(_, event)
    if event.joint then
        local s = get_stats():get("lab::LabStats")
        if s then
            s.detached_joints += 1
        end
    end
end, "FixedUpdate")

astral.system({
    name = "motor_driver",
    phase = "FixedUpdate",
    query = { "lab::MotorOscillator" },
    update = function(ctx, entity, osc)
        local target = math.sin(ctx.time * osc.frequency * 6.2831853) * osc.amplitude
        astral.physics.set_joint_motor_position(entity, target)
        local s = get_stats():get("lab::LabStats")
        if s then
            s.motor_updates += 1
        end
    end,
})

astral.system({
    name = "sensor_array",
    phase = "FixedUpdate",
    run = function(_)
        local queries: { RayQuery } = {
            { origin = vector.create(0, 12, 0), direction = vector.create(0, -1, 0), max_distance = 25 },
            { origin = vector.create(-6, 12, 0), direction = vector.create(0, -1, 0), max_distance = 25 },
            { origin = vector.create(6, 12, 0), direction = vector.create(0, -1, 0), max_distance = 25 },
            { origin = vector.create(80, 12, 0), direction = vector.create(0, -1, 0), max_distance = 25 },
        }
        local results = astral.physics.raycast_batch(queries)
        local count = 0
        for _, hit in ipairs(results) do
            if hit then
                count += 1
            end
        end
        local s = get_stats():get("lab::LabStats")
        if s then
            s.batch_ray_hits = count
        end
    end,
})

astral.system({
    name = "scene_character_driver",
    phase = "FixedUpdate",
    run = function()
        local character = astral.find("scene_character")
        if not character then
            return
        end
        local state = astral.physics.character_state(character)
        if not state then
            return
        end
        local transform = character:get("astral::Transform")
        if not transform then
            return
        end
        local x = transform.position.x
        local speed = if x < 0.9 then 1.5 else 0
        local jump = state.on_ground and x > -0.45 and x < -0.35
        astral.physics.move_character(character, vector.create(speed, 0, 0), jump)
    end,
})
