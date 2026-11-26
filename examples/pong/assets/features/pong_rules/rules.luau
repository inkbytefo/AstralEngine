--!strict
-- Pong's rules: your paddle, the AI, the ball, the depth ring. State lives in the components
-- (pong::Match, Paddle, Ball); this file is only behaviour. Scoring is in scoreboard.luau.

local function clamp(x: number, lo: number, hi: number): number
    return math.max(lo, math.min(hi, x))
end

-- Random number from a state stored in a component field (minstd: same seed, same match).
local function draw(owner: any, field: string): number
    local x = (owner[field] * 48271) % 2147483647
    owner[field] = x
    return (x - 1) / 2147483646
end

-- Position of a point bouncing between -limit and +limit after travelling x (mirror folding).
local function fold(x: number, limit: number): number
    if limit <= 0 then
        return 0
    end
    local period = 4 * limit
    local m = math.fmod(x + limit, period)
    if m < 0 then
        m = m + period
    end
    if m <= 2 * limit then
        return m - limit
    end
    return 3 * limit - m
end

local function face_z(court: any, side: string): number
    return side == "Player" and court.paddle_z or -court.paddle_z
end

local function clamp_to_court(p: vector, paddle: any, court: any): vector
    local hx = court.half_extents.x - paddle.half_size.x
    local hy = court.half_extents.y - paddle.half_size.y
    return vector.create(clamp(p.x, -hx, hx), clamp(p.y, -hy, hy), p.z)
end

-- The court entity's data (nil while no court is loaded).
local function court_and_match(): (any, any)
    local rows = astral.query({ "pong::Court", "pong::Match" })
    local row = rows[1]
    if not row then
        return nil, nil
    end
    return row[2], row[3]
end

-- First serve: seed the random states from the court's seed.
local function seed_match(court: any, m: any)
    if m.seeded then
        return
    end
    m.seeded = true
    m.serve_rng = math.max(1, court.seed)
    m.ai_rng = math.max(1, court.seed * 7919 + 1)
    m.serve_timer = court.serve_delay
end

-- Your paddle: WASD / arrows / gamepad ("move", "aim") or the mouse ("look"). Frame-rate based (input is per frame).
astral.system({
    name = "player_control",
    phase = "Update",
    query = { "pong::Paddle", "astral::Transform" },
    update = function(ctx, _, paddle, transform)
        if paddle.ai or paddle.side ~= "Player" then
            return
        end
        local court = court_and_match()
        if not court then
            return
        end
        local move = astral.input.axis("move")
        local aim = astral.input.axis("aim")
        local look = astral.input.axis("look")
        local p = transform.position
        local x = p.x + clamp(move.x + aim.x, -1, 1) * paddle.speed * ctx.delta + look.x
        local y = p.y + clamp(move.y + aim.y, -1, 1) * paddle.speed * ctx.delta + look.y
        transform.position = clamp_to_court(vector.create(x, y, p.z), paddle, court)
    end,
})

-- AI paddles: once the ball crosses into their half coming their way, slide to where it will arrive
-- (walls included), at speed * reaction; otherwise drift back to the middle. Each approach it picks
-- a spot on the paddle to hit with (to bend the ball) and misjudges the ball by up to
-- (1 - accuracy) * speed / 4 m: fast balls are harder to read.
astral.system({
    name = "ai_control",
    phase = "FixedUpdate",
    run = function(ctx)
        local court, m = court_and_match()
        if not court then
            return
        end
        seed_match(court, m)
        local ball_pos, ball_vel, radius = vector.create(0, 0, 0), vector.create(0, 0, 0), 0
        local ball = astral.query({ "pong::Ball", "astral::Transform" })[1]
        if ball then
            ball_pos, ball_vel, radius = ball[3].position, ball[2].velocity, ball[2].radius
        end
        local dt = ctx.fixed_delta
        for _, row in astral.query({ "pong::Paddle", "astral::Transform" }) do
            local paddle, transform = row[2], row[3]
            if not paddle.ai then
                continue
            end
            local sign = paddle.side == "Player" and 1 or -1
            local z = face_z(court, paddle.side)
            local coming = ball_vel.z * sign > 0 and ball_pos.z * sign > 0 -- noticed in its own half
            if coming and not paddle.coming then -- a new approach: choose the hit spot and the misjudgement
                local miss = (1 - clamp(paddle.accuracy, 0, 1)) * vector.magnitude(ball_vel) * 0.25
                local size = paddle.half_size
                local ox = (draw(m, "ai_rng") * 2 - 1) * size.x * 0.7 + (draw(m, "ai_rng") * 2 - 1) * miss
                local oy = (draw(m, "ai_rng") * 2 - 1) * size.y * 0.7 + (draw(m, "ai_rng") * 2 - 1) * miss
                paddle.plan_offset = { x = ox, y = oy }
            end
            paddle.coming = coming
            local target_x, target_y = 0, 0
            local speed = paddle.speed * paddle.reaction
            if coming then
                local offset = paddle.plan_offset
                local time = (z - ball_pos.z) / ball_vel.z
                target_x = fold(ball_pos.x + ball_vel.x * time, court.half_extents.x - radius) - offset.x
                target_y = fold(ball_pos.y + ball_vel.y * time, court.half_extents.y - radius) - offset.y
            else
                speed = speed * 0.4
            end
            local p = transform.position
            local to_x, to_y = target_x - p.x, target_y - p.y
            local dist = math.sqrt(to_x * to_x + to_y * to_y)
            local step = math.min(dist, speed * dt)
            if dist > 1e-5 then
                p = vector.create(p.x + to_x / dist * step, p.y + to_y / dist * step, p.z)
            end
            transform.position = clamp_to_court(p, paddle, court)
        end
    end,
})

-- Serves, moves and bounces the ball; scores when it gets past a paddle. Fixed rate: deterministic.
astral.system({
    name = "ball_physics",
    phase = "FixedUpdate",
    run = function(ctx)
        local court, m = court_and_match()
        if not court then
            return
        end
        seed_match(court, m)
        local dt = ctx.fixed_delta
        if m.game_over_timer > 0 then
            m.game_over_timer = m.game_over_timer - dt
            if m.game_over_timer <= 0 then -- a new match
                m.player = 0
                m.opponent = 0
                m.serve_timer = court.serve_delay
            end
            return
        end

        local paddles = {}
        for _, row in astral.query({ "pong::Paddle", "astral::Transform" }) do
            table.insert(paddles, { side = row[2].side, half = row[2].half_size, pos = row[3].position })
        end

        local scored, bounces = {}, {}
        for _, row in astral.query({ "pong::Ball", "astral::Transform" }) do
            local ball, transform = row[2], row[3]
            local v = ball.velocity
            if v.x == 0 and v.y == 0 and v.z == 0 then -- waiting for the serve
                transform.position = vector.create(0, 0, 0)
                m.serve_timer = m.serve_timer - dt
                if m.serve_timer > 0 then
                    continue
                end
                local sx = -0.35 + 0.7 * draw(m, "serve_rng")
                local sy = -0.25 + 0.5 * draw(m, "serve_rng")
                local dir = vector.normalize(vector.create(sx, sy, m.serve_to == "Player" and 1 or -1))
                ball.velocity = dir * ball.serve_speed
                table.insert(bounces, { on = "Serve", position = transform.position, speed = ball.serve_speed })
                continue
            end

            local start = transform.position
            local radius = ball.radius
            local px: number, py: number, pz: number = start.x + v.x * dt, start.y + v.y * dt, start.z + v.z * dt
            local vx: number, vy: number, vz: number = v.x, v.y, v.z
            -- Tunnel walls.
            local lx, ly = court.half_extents.x - radius, court.half_extents.y - radius
            local wall = false
            if px > lx then px = 2 * lx - px; vx = -math.abs(vx); wall = true end
            if px < -lx then px = -2 * lx - px; vx = math.abs(vx); wall = true end
            if py > ly then py = 2 * ly - py; vy = -math.abs(vy); wall = true end
            if py < -ly then py = -2 * ly - py; vy = math.abs(vy); wall = true end
            if wall then
                local speed = vector.magnitude(vector.create(vx, vy, vz))
                table.insert(bounces, { on = "Wall", position = vector.create(px, py, pz), speed = speed })
            end
            -- Paddles: crossing the paddle's face this step, inside its rectangle (swept: no tunnelling).
            for _, pa in paddles do
                local sign = pa.side == "Player" and 1 or -1
                if vz * sign <= 0 then
                    continue
                end
                local plane = face_z(court, pa.side) - sign * radius
                if (start.z - plane) * sign > 0 or (pz - plane) * sign < 0 then
                    continue
                end
                local dx, dy = px - pa.pos.x, py - pa.pos.y
                if math.abs(dx) > pa.half.x + radius or math.abs(dy) > pa.half.y + radius then
                    continue
                end
                local speed = math.min(vector.magnitude(vector.create(vx, vy, vz)) * ball.speedup, ball.max_speed)
                vx = vx + clamp(dx / pa.half.x, -1, 1) * ball.spin * speed
                vy = vy + clamp(dy / pa.half.y, -1, 1) * ball.spin * speed
                vz = -vz
                local len = vector.magnitude(vector.create(vx, vy, vz))
                vx, vy, vz = vx / len * speed, vy / len * speed, vz / len * speed
                if math.abs(vz) < 0.45 * speed then -- keep the ball travelling down the tunnel
                    local nvz = -sign * 0.45 * speed
                    local xy = math.sqrt(math.max(0, speed * speed - nvz * nvz) / math.max(1e-6, vx * vx + vy * vy))
                    vx, vy, vz = vx * xy, vy * xy, nvz
                end
                pz = 2 * plane - pz
                m.rallies = m.rallies + 1
                table.insert(bounces, { on = "Paddle", position = vector.create(px, py, pz), speed = speed })
                break
            end
            -- Past a paddle: a point for the other side.
            if math.abs(pz) > court.half_extents.z + radius then
                local conceded = pz > 0 and "Player" or "Opponent"
                table.insert(scored, conceded == "Player" and "Opponent" or "Player")
                m.serve_to = conceded
                m.serve_timer = court.serve_delay
                px, py, pz, vx, vy, vz = 0, 0, 0, 0, 0, 0
            end
            transform.position = vector.create(px, py, pz)
            ball.velocity = vector.create(vx, vy, vz)
        end
        for _, side in scored do
            astral.send("pong::Scored", { by = side })
        end
        for _, bounce in bounces do
            astral.send("pong::Bounced", bounce)
        end
    end,
})

-- The depth ring sits at the ball's depth.
astral.system({
    name = "depth_follow",
    phase = "PostUpdate",
    run = function()
        local z = 0
        local ball = astral.query({ "pong::Ball", "astral::Transform" })[1]
        if ball then
            z = ball[3].position.z
        end
        for _, row in astral.query({ "pong::DepthMarker", "astral::Transform" }) do
            local p = row[3].position
            row[3].position = vector.create(p.x, p.y, z)
        end
    end,
})
