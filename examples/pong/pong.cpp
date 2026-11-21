#include "pong.hpp"

#include <algorithm>
#include <cmath>

using namespace astral;

namespace pong {

namespace {

const Court* find_court(World& w) {
    const Court* court = nullptr;
    w.each<const Court>([&](const Court& c) {
        if (!court) court = &c;
    });
    return court;
}

/// Position of a point bouncing between -limit and +limit after travelling `x` (mirror folding).
float fold(float x, float limit) {
    if (limit <= 0) return 0;
    const float period = 4 * limit;
    float m = std::fmod(x + limit, period);
    if (m < 0) m += period;
    return m <= 2 * limit ? m - limit : 3 * limit - m;
}

float face_z(const Court& c, Side side) { return side == Side::Player ? c.paddle_z : -c.paddle_z; }

void clamp_to_court(Vec3& p, const Paddle& paddle, const Court& c) {
    p.x = std::clamp(p.x, -(c.half_extents.x - paddle.half_size.x), c.half_extents.x - paddle.half_size.x);
    p.y = std::clamp(p.y, -(c.half_extents.y - paddle.half_size.y), c.half_extents.y - paddle.half_size.y);
}

/// Your paddle: WASD / arrows, or the mouse. Frame-rate based (input is per frame).
class PlayerControl final : public System {
public:
    void update(Context& ctx) override {
        const Court* court = find_court(ctx.world);
        if (!court) return;
        const Input& in = ctx.input;
        Vec2 move = in.wasd();
        move.x += (in.down(Key::Right) ? 1.0f : 0.0f) - (in.down(Key::Left) ? 1.0f : 0.0f);
        move.y += (in.down(Key::Up) ? 1.0f : 0.0f) - (in.down(Key::Down) ? 1.0f : 0.0f);
        const Vec2 mouse = in.mouse_delta() * 0.012f;
        ctx.world.each<const Paddle, Transform>([&](const Paddle& p, Transform& t) {
            if (p.ai || p.side != Side::Player) return;
            t.position.x += std::clamp(move.x, -1.0f, 1.0f) * p.speed * ctx.time.delta + mouse.x;
            t.position.y += std::clamp(move.y, -1.0f, 1.0f) * p.speed * ctx.time.delta - mouse.y;
            clamp_to_court(t.position, p, *court);
        });
    }
};

/// AI paddles: once the ball crosses into their half coming their way, slide to where it will arrive (walls included),
/// at `speed * reaction`; otherwise drift back to the middle. Each approach it picks a spot on the
/// paddle to hit with (to bend the ball) and misjudges the ball by up to (1 - accuracy) * speed / 4 m:
/// fast balls are harder to read.
class AiControl final : public System {
public:
    explicit AiControl(bool player_ai) : player_ai_(player_ai) {}
    void update(Context& ctx) override {
        const Court* court = find_court(ctx.world);
        if (!court) return;
        Vec3 ball_pos{}, ball_vel{};
        float radius = 0;
        ctx.world.each<const Ball, const Transform>([&](const Ball& b, const Transform& t) {
            ball_pos = t.position;
            ball_vel = b.velocity;
            radius = b.radius;
        });
        const float dt = ctx.time.fixed_delta;
        if (!seeded_) {
            rng_.seed(court->seed * 7919u + 1u);
            seeded_ = true;
        }
        ctx.world.each<const Paddle, Transform>([&](const Paddle& p, Transform& t) {
            if (!p.ai && !(player_ai_ && p.side == Side::Player)) return;
            const float z = face_z(*court, p.side);
            const float sign = p.side == Side::Player ? 1.0f : -1.0f;
            const bool coming = ball_vel.z * sign > 0 && ball_pos.z * sign > 0; // noticed in its own half
            Plan& plan = plans_[static_cast<int>(p.side)];
            if (coming && !plan.coming) { // a new approach: choose the hit spot and the misjudgement
                std::uniform_real_distribution<float> u(-1.0f, 1.0f);
                const float miss = (1.0f - std::clamp(p.accuracy, 0.0f, 1.0f)) * length(ball_vel) * 0.25f;
                plan.offset = {u(rng_) * p.half_size.x * 0.7f + u(rng_) * miss, u(rng_) * p.half_size.y * 0.7f + u(rng_) * miss};
            }
            plan.coming = coming;
            Vec2 target{0, 0};
            float speed = p.speed * p.reaction;
            if (coming) {
                const float time = (z - ball_pos.z) / ball_vel.z;
                target = {fold(ball_pos.x + ball_vel.x * time, court->half_extents.x - radius) - plan.offset.x,
                          fold(ball_pos.y + ball_vel.y * time, court->half_extents.y - radius) - plan.offset.y};
            } else {
                speed *= 0.4f;
            }
            const Vec2 to{target.x - t.position.x, target.y - t.position.y};
            const float dist = std::sqrt(to.x * to.x + to.y * to.y);
            const float step = std::min(dist, speed * dt);
            if (dist > 1e-5f) {
                t.position.x += to.x / dist * step;
                t.position.y += to.y / dist * step;
            }
            clamp_to_court(t.position, p, *court);
        });
    }

private:
    struct Plan {
        bool coming = false;
        Vec2 offset{};
    };
    bool player_ai_;
    Plan plans_[2];
    std::minstd_rand rng_;
    bool seeded_ = false;
};

/// Serves, moves and bounces the ball; scores when it gets past a paddle. Fixed rate: deterministic.
class BallPhysics final : public System {
public:
    void update(Context& ctx) override {
        World& w = ctx.world;
        Match& m = *w.resource<Match>();
        const Court* court_ptr = find_court(w);
        if (!court_ptr) return;
        const Court court = *court_ptr;
        const float dt = ctx.time.fixed_delta;
        if (!m.seeded) {
            m.rng.seed(court.seed);
            m.seeded = true;
            m.serve_timer = court.serve_delay;
        }
        if (m.game_over_timer > 0) {
            m.game_over_timer -= dt;
            if (m.game_over_timer <= 0) { // new match
                m.player = m.opponent = 0;
                m.serve_timer = court.serve_delay;
            }
            return;
        }
        struct PaddleAt {
            Paddle paddle;
            Vec3 position;
        };
        std::vector<PaddleAt> paddles;
        w.each<const Paddle, const Transform>([&](const Paddle& p, const Transform& t) { paddles.push_back({p, t.position}); });

        std::vector<Side> scored;
        std::vector<Bounced> bounces;
        w.each<Ball, Transform>([&](Ball& b, Transform& t) {
            Vec3 v = b.velocity;
            if (v.x == 0 && v.y == 0 && v.z == 0) { // waiting for the serve
                t.position = {};
                m.serve_timer -= dt;
                if (m.serve_timer > 0) return;
                std::uniform_real_distribution<float> sx(-0.35f, 0.35f), sy(-0.25f, 0.25f);
                const Vec3 dir = normalize(Vec3{sx(m.rng), sy(m.rng), m.serve_to == Side::Player ? 1.0f : -1.0f});
                b.velocity = dir * b.serve_speed;
                bounces.push_back({Bounced::On::Serve, t.position, b.serve_speed});
                return;
            }
            Vec3 p = t.position + v * dt;
            // Tunnel walls.
            const float lx = court.half_extents.x - b.radius, ly = court.half_extents.y - b.radius;
            bool wall = false;
            if (p.x > lx) { p.x = 2 * lx - p.x; v.x = -std::abs(v.x); wall = true; }
            if (p.x < -lx) { p.x = -2 * lx - p.x; v.x = std::abs(v.x); wall = true; }
            if (p.y > ly) { p.y = 2 * ly - p.y; v.y = -std::abs(v.y); wall = true; }
            if (p.y < -ly) { p.y = -2 * ly - p.y; v.y = std::abs(v.y); wall = true; }
            if (wall) bounces.push_back({Bounced::On::Wall, p, length(v)});
            // Paddles: crossing the paddle's face this step, inside its rectangle (swept: no tunnelling).
            for (const PaddleAt& pa : paddles) {
                const float sign = pa.paddle.side == Side::Player ? 1.0f : -1.0f;
                if (v.z * sign <= 0) continue;
                const float plane = face_z(court, pa.paddle.side) - sign * b.radius;
                if ((t.position.z - plane) * sign > 0 || (p.z - plane) * sign < 0) continue;
                const float dx = p.x - pa.position.x, dy = p.y - pa.position.y;
                if (std::abs(dx) > pa.paddle.half_size.x + b.radius || std::abs(dy) > pa.paddle.half_size.y + b.radius) continue;
                const float speed = std::min(length(v) * b.speedup, b.max_speed);
                v.x += std::clamp(dx / pa.paddle.half_size.x, -1.0f, 1.0f) * b.spin * speed;
                v.y += std::clamp(dy / pa.paddle.half_size.y, -1.0f, 1.0f) * b.spin * speed;
                v.z = -v.z;
                v = normalize(v) * speed;
                if (std::abs(v.z) < 0.45f * speed) { // keep the ball travelling down the tunnel
                    const float vz = -sign * 0.45f * speed;
                    const float xy = std::sqrt(std::max(0.0f, speed * speed - vz * vz) / std::max(1e-6f, v.x * v.x + v.y * v.y));
                    v = {v.x * xy, v.y * xy, vz};
                }
                p.z = 2 * plane - p.z;
                ++m.rallies;
                bounces.push_back({Bounced::On::Paddle, p, speed});
                break;
            }
            // Past a paddle: a point for the other side.
            if (std::abs(p.z) > court.half_extents.z + b.radius) {
                const Side conceded = p.z > 0 ? Side::Player : Side::Opponent;
                scored.push_back(conceded == Side::Player ? Side::Opponent : Side::Player);
                m.serve_to = conceded;
                m.serve_timer = court.serve_delay;
                p = {};
                v = {};
            }
            t.position = p;
            b.velocity = v;
        });
        for (Side s : scored) w.send(Scored{s});
        for (const Bounced& b : bounces) w.send(b);
    }
};

/// Keeps the score, ends matches, lights the score pips.
class Scoreboard final : public System {
public:
    void update(Context& ctx) override {
        World& w = ctx.world;
        Match& m = *w.resource<Match>();
        const Court* court = find_court(w);
        events_.read(w, [&](const Scored& s) {
            (s.by == Side::Player ? m.player : m.opponent) += 1;
            if (court && std::max(m.player, m.opponent) >= court->points_to_win) m.game_over_timer = 3.0f;
        });
        std::vector<std::pair<Entity, Vec3>> changes;
        w.each<const ScorePip, const PrimitiveMesh>([&](Entity e, const ScorePip& pip, const PrimitiveMesh& mesh) {
            const bool lit = (pip.side == Side::Player ? m.player : m.opponent) > pip.index;
            const Vec3 emissive = pip.color * (lit ? 6.0f : 0.03f);
            const Vec3& now = mesh.material.emissive;
            if (now.x != emissive.x || now.y != emissive.y || now.z != emissive.z) changes.emplace_back(e, emissive);
        });
        for (const auto& [e, emissive] : changes) w.get<PrimitiveMesh>(e)->material.emissive = emissive; // only real changes
    }

private:
    EventReader<Scored> events_;
};

/// Plays the match: bounces at the ball (3D, faster = higher), points and results flat.
class MatchAudio final : public System {
public:
    void update(Context& ctx) override {
        World& w = ctx.world;
        const Sounds* sounds = nullptr;
        w.each<const Sounds>([&](const Sounds& s) { sounds = &s; });
        if (!sounds || !ctx.audio) {
            bounces_.read(w, [](const Bounced&) {});
            scores_.read(w, [](const Scored&) {});
            return;
        }
        const Sounds s = *sounds;
        auto play = [&](const std::string& path, PlayDesc desc) {
            if (path.empty()) return;
            desc.volume *= s.volume;
            ctx.audio->play(ctx.sound(path), desc);
        };
        int walls = 0;
        bounces_.read(w, [&](const Bounced& b) {
            const float pitch = std::clamp(0.85f + b.speed / 40.0f, 0.85f, 1.5f);
            const PlayDesc at_ball{.pitch = pitch, .spatial = true, .position = b.position, .min_distance = 4.0f, .max_distance = 40.0f};
            switch (b.on) {
            case Bounced::On::Serve: play(s.serve, {.volume = 0.7f}); break;
            case Bounced::On::Paddle: play(s.paddle, at_ball); break;
            case Bounced::On::Wall: // a corner is two walls in one step: one click
                if (walls++ == 0) {
                    PlayDesc d = at_ball;
                    d.volume = 0.7f;
                    play(s.wall, d);
                }
                break;
            }
        });
        const Match& m = *w.resource<Match>();
        scores_.read(w, [&](const Scored& e) {
            const bool won = e.by == Side::Player;
            if (m.game_over_timer > 0) play(m.player > m.opponent ? s.match_won : s.match_lost, {.bus = AudioBus::Interface});
            else play(won ? s.point_won : s.point_lost, {.bus = AudioBus::Interface});
        });
    }

private:
    EventReader<Bounced> bounces_;
    EventReader<Scored> scores_;
};

/// The depth ring sits at the ball's depth.
class DepthFollow final : public System {
public:
    void update(Context& ctx) override {
        float z = 0;
        ctx.world.each<const Ball, const Transform>([&](const Ball&, const Transform& t) { z = t.position.z; });
        ctx.world.each<const DepthMarker, Transform>([&](const DepthMarker&, Transform& t) { t.position.z = z; });
    }
};

} // namespace

void install(Context& ctx, bool player_ai) {
    ctx.types.add<Court>();
    ctx.types.add<Paddle>();
    ctx.types.add<Ball>();
    ctx.types.add<ScorePip>();
    ctx.types.add<DepthMarker>();
    ctx.types.add<Sounds>();
    ctx.world.set_resource(Match{});
    if (!player_ai) ctx.add_system<PlayerControl>(Phase::Update);
    ctx.add_system<AiControl>(Phase::FixedUpdate, player_ai);
    ctx.add_system<BallPhysics>(Phase::FixedUpdate);
    ctx.add_system<Scoreboard>(Phase::PostUpdate);
    ctx.add_system<DepthFollow>(Phase::PostUpdate);
    ctx.add_system<MatchAudio>(Phase::PostUpdate); // after the Scoreboard: knows when a match ends
}

const Match* match(World& world) { return world.resource<Match>(); }

} // namespace pong
