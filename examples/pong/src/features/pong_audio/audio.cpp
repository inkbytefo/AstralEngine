#include "features/pong_audio/sounds.hpp"

#include "features/pong_rules/types.hpp"

using namespace astral;

namespace pong {

namespace {

/// Plays the match: bounces at the ball (3D, faster = higher), points and results flat.
class MatchAudio final : public System {
public:
    void update(Context& ctx) override {
        World& w = ctx.world;
        const Sounds* sounds = ctx.singleton<Sounds>();
        if (!sounds || !ctx.audio) {
            bounces_.read(w, [](const Bounced&) {});
            scores_.read(w, [](const Scored&) {});
            return;
        }
        const Sounds s = *sounds;
        auto play = [&](const std::string& path, PlayDesc desc) {
            if (path.empty()) return;
            desc.volume *= s.volume;
            ctx.audio->play(ctx.audio_service().sound(path), desc);
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
        const Match* match = ctx.singleton<Match>();
        scores_.read(w, [&](const Scored& e) {
            const bool won = e.by == Side::Player;
            if (match && match->game_over_timer > 0) play(match->player > match->opponent ? s.match_won : s.match_lost, {.bus = AudioBus::Interface});
            else play(won ? s.point_won : s.point_lost, {.bus = AudioBus::Interface});
        });
    }

private:
    EventReader<Bounced> bounces_;
    EventReader<Scored> scores_;
};

} // namespace

void install_audio(FeatureContext& feature) {
    feature.component<Sounds>();
    feature.system<MatchAudio>(Phase::PostUpdate); // after the scores: knows when a match ends
}

} // namespace pong
