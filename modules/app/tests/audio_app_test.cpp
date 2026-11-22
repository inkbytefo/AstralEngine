#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <cmath>

using namespace astral;

namespace {
std::vector<std::uint8_t> tone_wav(float seconds) {
    const std::uint32_t rate = 48000, frames = static_cast<std::uint32_t>(seconds * 48000.0f);
    std::vector<std::uint8_t> out;
    auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    auto u16 = [&](std::uint16_t v) { for (int i = 0; i < 2; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    auto tag = [&](const char* t) { out.insert(out.end(), t, t + 4); };
    tag("RIFF"); u32(36 + frames * 2); tag("WAVE"); tag("fmt "); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    tag("data"); u32(frames * 2);
    for (std::uint32_t i = 0; i < frames; ++i) u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::sin(static_cast<float>(i) * 0.06f) * 20000.0f)));
    return out;
}
} // namespace

TEST_CASE("AudioSource components play, follow their entity, end and stop; ctx.sound caches") {
    const auto dir = test::temp_dir("app_audio");
    std::filesystem::create_directories(dir / "sounds");
    test::write_bytes(dir / "sounds" / "blip.wav", tone_wav(0.1f));
    test::write_bytes(dir / "sounds" / "loop.wav", tone_wav(0.5f));
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = false});
    struct G final : Game {
        Entity blip{}, loop{};
        int frame = 0;
        std::uint32_t voices_at_1 = 0, voices_at_20 = 0, voices_at_30 = 0;
        bool blip_playing_at_20 = true, cached = false, missing_invalid = false;
        void on_start(Context& ctx) override {
            REQUIRE(ctx.audio);
            blip = ctx.world.create(Transform{}, AudioSource{.sound = "sounds/blip.wav", .bus = AudioBus::Effects, .loop = false});
            loop = ctx.world.create(Transform{.position = {3, 0, 0}}, AudioSource{.sound = "sounds/loop.wav"});
            cached = ctx.audio_service().sound("sounds/blip.wav") == ctx.audio_service().sound("sounds/blip.wav");
            missing_invalid = !ctx.audio_service().sound("sounds/nope.wav").valid();
        }
        void on_update(Context& ctx) override {
            ++frame;
            // The null device only mixes when read: pull this frame's audio like a device would.
            std::vector<float> mix(static_cast<std::size_t>(ctx.audio->sample_rate() / 60) * ctx.audio->channels());
            ctx.audio->read(mix);
            if (frame == 2) voices_at_1 = ctx.audio->voice_count(); // sources start at the end of frame 1
            if (frame == 20) {
                voices_at_20 = ctx.audio->voice_count();
                blip_playing_at_20 = ctx.world.get<AudioSource>(blip)->playing;
                ctx.world.remove<AudioSource>(loop);
            }
            if (frame == 30) voices_at_30 = ctx.audio->voice_count();
        }
    } game;
    app.run_frames(game, 31);
    CHECK(game.cached);
    CHECK(game.missing_invalid);
    CHECK(game.voices_at_1 == 2);
    CHECK(game.voices_at_20 == 1);        // the 0.1 s blip ended...
    CHECK_FALSE(game.blip_playing_at_20); // ...and says so
    CHECK(game.voices_at_30 == 0);        // the loop stopped with its component
}
