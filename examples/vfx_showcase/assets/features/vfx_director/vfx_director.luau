--!strict
-- Luau rules for vfx_showcase: drives PlayVfx / StopVfx / DisperseSmoke events and spatial AudioSource one-shots.

local function spawn_one_shot_sound(pos: vector, sound_path: string, volume: number)
    if sound_path == "" then
        return
    end
    astral.spawn({
        ["astral::Transform"] = { position = pos },
        ["astral::AudioSource"] = {
            sound = sound_path,
            bus = "Effects",
            volume = volume,
            pitch = 1.0,
            loop = false,
            spatial = true,
            min_distance = 2.0,
            max_distance = 45.0,
            playing = true,
        },
        ["vfx_director::SoundOneShot"] = { max_age = 3.0, age = 0.0 },
    })
end

-- Interactive input actions: Space triggers a burst + spatial cue sound, X stops tagged VFX,
-- C disperses gameplay smoke along the configured capsule with a spatial whoosh sound.
astral.system({
    name = "vfx_actions",
    phase = "Update",
    run = function()
        if astral.input.pressed("trigger_vfx") then
            astral.each({ "astral::Transform", "vfx_director::Beacon" }, function(_, transform, beacon)
                astral.send("astral::PlayVfx", {
                    asset = beacon.vfx_asset,
                    position = transform.position,
                    tag = beacon.tag,
                })
                beacon.bursts_fired += 1
                beacon.timer = beacon.interval
                if beacon.cue_sound ~= "" then
                    spawn_one_shot_sound(transform.position, beacon.cue_sound, beacon.sound_volume)
                    beacon.sounds_fired += 1
                end
            end)
        end

        if astral.input.pressed("stop_vfx") then
            astral.each({ "vfx_director::Beacon" }, function(_, beacon)
                if beacon.tag ~= "" then
                    astral.send("astral::StopVfx", { tag = beacon.tag })
                    beacon.stops_sent += 1
                end
            end)
        end

        if astral.input.pressed("disperse_smoke") then
            astral.each({ "vfx_director::SmokeDisperser" }, function(_, disperser)
                astral.send("astral::DisperseSmoke", {
                    from = disperser.from,
                    to = disperser.to,
                    radius_m = disperser.radius_m,
                    hold_steps = disperser.hold_steps,
                })
                disperser.pulses_fired += 1
                local mid = (disperser.from + disperser.to) * 0.5
                spawn_one_shot_sound(mid, disperser.sound, disperser.sound_volume)
            end)
        end
    end,
})

-- Deterministic timed VFX beacons on FixedUpdate.
astral.system({
    name = "vfx_beacons",
    phase = "FixedUpdate",
    query = { "astral::Transform", "vfx_director::Beacon" },
    update = function(ctx, _, transform, beacon)
        if not beacon.active then
            return
        end
        if beacon.timer <= 0.0 then
            astral.send("astral::PlayVfx", {
                asset = beacon.vfx_asset,
                position = transform.position,
                tag = beacon.tag,
            })
            beacon.bursts_fired += 1
            if beacon.play_cue_sound and beacon.cue_sound ~= "" then
                spawn_one_shot_sound(transform.position, beacon.cue_sound, beacon.sound_volume)
                beacon.sounds_fired += 1
            end
            if beacon.interval > 0.0 then
                beacon.timer = beacon.interval
            else
                beacon.active = false
            end
        elseif beacon.interval > 0.0 then
            beacon.timer -= ctx.fixed_delta
        end
    end,
})

-- Clean up Luau-spawned one-shot AudioSource entities after playback finishes.
astral.system({
    name = "sound_oneshot_cleanup",
    phase = "FixedUpdate",
    query = { "astral::AudioSource", "vfx_director::SoundOneShot" },
    update = function(ctx, entity, source, oneshot)
        oneshot.age += ctx.fixed_delta
        if (oneshot.age >= ctx.fixed_delta and not source.playing) or oneshot.age >= oneshot.max_age then
            entity:destroy()
        end
    end,
})
