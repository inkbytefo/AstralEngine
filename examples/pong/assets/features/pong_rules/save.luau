--!strict
-- Match save/load through the engine's save slots (astral.save / astral.load / astral.has_save).
-- Snapshots live on a dedicated pong::MatchSave entity so astral.load only replaces the snapshot
-- and leaves the court scene's lights, meshes, audio, and paddle child hierarchies untouched.

local SLOT = "match"

local function find_match_entities(): (any, any, any, any, any, any, any)
    local court_row = astral.query({ "pong::Court", "pong::Match" })[1]
    local ball_row = astral.query({ "pong::Ball", "astral::Transform" })[1]
    if not court_row or not ball_row then
        return nil, nil, nil, nil, nil, nil, nil
    end
    local player_p: any, player_t: any, opp_p: any, opp_t: any = nil, nil, nil, nil
    for _, row in astral.query({ "pong::Paddle", "astral::Transform" }) do
        if row[2].side == "Player" then
            player_p, player_t = row[2], row[3]
        else
            opp_p, opp_t = row[2], row[3]
        end
    end
    if not player_p or not opp_p then
        return nil, nil, nil, nil, nil, nil, nil
    end
    return court_row[3], ball_row[2], ball_row[3], player_p, player_t, opp_p, opp_t
end

local function write_snapshot(snap: any, m: any, ball: any, ball_t: any, player_p: any, player_t: any, opp_p: any, opp_t: any)
    snap.player = m.player
    snap.opponent = m.opponent
    snap.serve_timer = m.serve_timer
    snap.serve_to = m.serve_to
    snap.game_over_timer = m.game_over_timer
    snap.rallies = m.rallies
    snap.serve_rng = m.serve_rng
    snap.ai_rng = m.ai_rng
    snap.seeded = m.seeded
    snap.ball_position = ball_t.position
    snap.ball_velocity = ball.velocity
    snap.player_position = player_t.position
    snap.player_coming = player_p.coming
    snap.player_plan_offset = player_p.plan_offset
    snap.opponent_position = opp_t.position
    snap.opponent_coming = opp_p.coming
    snap.opponent_plan_offset = opp_p.plan_offset
end

local function apply_snapshot(snap: any, m: any, ball: any, ball_t: any, player_p: any, player_t: any, opp_p: any, opp_t: any)
    m.player = snap.player
    m.opponent = snap.opponent
    m.serve_timer = snap.serve_timer
    m.serve_to = snap.serve_to
    m.game_over_timer = snap.game_over_timer
    m.rallies = snap.rallies
    m.serve_rng = snap.serve_rng
    m.ai_rng = snap.ai_rng
    m.seeded = snap.seeded
    ball_t.position = snap.ball_position
    ball.velocity = snap.ball_velocity
    player_t.position = snap.player_position
    player_p.coming = snap.player_coming
    player_p.plan_offset = snap.player_plan_offset
    opp_t.position = snap.opponent_position
    opp_p.coming = snap.opponent_coming
    opp_p.plan_offset = snap.opponent_plan_offset
end

astral.system({
    name = "match_save",
    phase = "Update",
    run = function()
        local want_save = astral.input.pressed("save_match")
        local want_load = astral.input.pressed("load_match")
        if not want_save and not want_load then
            return
        end
        local m, ball, ball_t, player_p, player_t, opp_p, opp_t = find_match_entities()
        if not m then
            return
        end
        if want_save then
            local row = astral.query({ "pong::MatchSave" })[1]
            local snap: any = row and row[2] or nil
            if not snap then
                local entity = astral.spawn({ ["pong::MatchSave"] = {} })
                snap = entity:get("pong::MatchSave")
            end
            if not snap then
                return
            end
            write_snapshot(snap, m, ball, ball_t, player_p, player_t, opp_p, opp_t)
            if astral.save(SLOT, { "pong::MatchSave" }) then
                astral.send("pong::SaveNotice", { kind = "Saved" })
            end
        elseif want_load then
            if not astral.has_save(SLOT) then
                astral.send("pong::SaveNotice", { kind = "Missing" })
                return
            end
            if astral.load(SLOT) then
                local row = astral.query({ "pong::MatchSave" })[1]
                if row then
                    apply_snapshot(row[2], m, ball, ball_t, player_p, player_t, opp_p, opp_t)
                    astral.send("pong::SaveNotice", { kind = "Loaded" })
                end
            end
        end
    end,
})
