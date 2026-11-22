--!strict
-- Rule: the camera looks and flies by the input actions. Yaw and pitch live in the component.

local UP = vector.create(0, 1, 0)
local RIGHT = vector.create(1, 0, 0)

astral.system({
    name = "fly_camera",
    phase = "Update",
    query = { "astral::Transform", "fly::FlyCamera" },
    update = function(ctx, _, transform, cam)
        if cam.autopilot <= 0 and (astral.input.down("look_hold") or not cam.look_needs_button) then
            local look = astral.input.axis("look")
            cam.yaw = cam.yaw - look.x * cam.look_speed
            cam.pitch = math.max(-1.4, math.min(1.4, cam.pitch - look.y * cam.look_speed))
        end
        local rotation = astral.quat.mul(astral.quat.axis_angle(UP, cam.yaw), astral.quat.axis_angle(RIGHT, cam.pitch))
        transform.rotation = rotation

        local direction, speed
        if cam.autopilot > 0 then
            direction, speed = vector.create(0, 0, -1), cam.autopilot
        else
            local move = astral.input.axis("move")
            direction = vector.create(move.x, astral.input.value("rise"), -move.y)
            speed = astral.input.down("fast") and cam.fast_speed or cam.speed
        end
        local step = astral.quat.rotate(rotation, direction)
        transform.position = transform.position + step * speed * ctx.delta
    end,
})
