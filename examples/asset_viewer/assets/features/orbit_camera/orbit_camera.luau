--!strict
-- Rule: orbits the camera around target using arrow keys or left-mouse drag, zooms with +/- keys.
-- Mouse scroll zoom runs in the C++ orbit_camera_scroll system (InputActions has no scroll axis).

local UP = vector.create(0, 1, 0)
local RIGHT = vector.create(1, 0, 0)

astral.system({
    name = "orbit_camera",
    phase = "Update",
    query = { "astral::Transform", "orbit::OrbitCamera" },
    update = function(ctx, _, transform, cam)
        if not cam.enabled then
            return
        end

        local keys = astral.input.axis("orbit_keys")
        cam.yaw = cam.yaw - keys.x * cam.orbit_speed * ctx.delta
        cam.pitch = cam.pitch + keys.y * cam.orbit_speed * ctx.delta

        if astral.input.down("orbit_drag") then
            local delta = astral.input.axis("orbit_delta")
            cam.yaw = cam.yaw - delta.x * cam.drag_speed
            cam.pitch = cam.pitch - delta.y * cam.drag_speed
        end

        cam.pitch = math.max(-1.45, math.min(1.45, cam.pitch))

        local zoom = astral.input.value("orbit_zoom")
        if zoom ~= 0 then
            local factor = math.exp(zoom * cam.zoom_speed * ctx.delta)
            cam.distance = math.max(cam.min_distance, math.min(cam.max_distance, cam.distance * factor))
        end

        local rotation = astral.quat.mul(astral.quat.axis_angle(UP, cam.yaw), astral.quat.axis_angle(RIGHT, cam.pitch))
        transform.rotation = rotation
        transform.position = cam.target + astral.quat.rotate(rotation, vector.create(0, 0, cam.distance))
    end,
})
