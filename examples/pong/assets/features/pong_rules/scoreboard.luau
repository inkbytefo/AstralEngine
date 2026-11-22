--!strict
-- Keeps the score and ends matches. Loaded after rules.luau so it hears the ball's points in the same
-- fixed step (modules run in the order they are loaded).

astral.on("pong::Scored", function(_, event)
    local row = astral.query({ "pong::Court", "pong::Match" })[1]
    if not row then
        return
    end
    local court, m = row[2], row[3]
    if event.by == "Player" then
        m.player = m.player + 1
    else
        m.opponent = m.opponent + 1
    end
    if math.max(m.player, m.opponent) >= court.points_to_win then
        m.game_over_timer = 3.0
    end
end, "FixedUpdate")
