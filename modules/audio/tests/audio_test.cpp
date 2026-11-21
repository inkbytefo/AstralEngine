#include <astral/audio/audio.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <cstring>
#include <vector>

using namespace astral;

namespace {

/// A 16-bit PCM WAV file in memory: a sine tone.
std::vector<std::uint8_t> sine_wav(float seconds, float hz = 440.0f, std::uint32_t rate = 48000) {
    const auto frames = static_cast<std::uint32_t>(seconds * static_cast<float>(rate));
    std::vector<std::uint8_t> out;
    auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    auto u16 = [&](std::uint16_t v) { for (int i = 0; i < 2; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    auto tag = [&](const char* t) { out.insert(out.end(), t, t + 4); };
    tag("RIFF");
    u32(36 + frames * 2);
    tag("WAVE");
    tag("fmt ");
    u32(16);
    u16(1); // PCM
    u16(1); // mono
    u32(rate);
    u32(rate * 2);
    u16(2);
    u16(16);
    tag("data");
    u32(frames * 2);
    for (std::uint32_t i = 0; i < frames; ++i) {
        const float s = std::sin(2.0f * 3.14159265f * hz * static_cast<float>(i) / static_cast<float>(rate)) * 0.8f;
        u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(s * 32767.0f)));
    }
    return out;
}

/// RMS of each channel over `seconds` of mixed output.
std::pair<float, float> loudness(AudioEngine& a, float seconds) {
    std::vector<float> buffer(static_cast<std::size_t>(seconds * static_cast<float>(a.sample_rate())) * a.channels());
    const std::size_t frames = a.read(buffer);
    double l = 0, r = 0;
    for (std::size_t i = 0; i < frames; ++i) {
        l += double(buffer[i * 2]) * buffer[i * 2];
        r += double(buffer[i * 2 + 1]) * buffer[i * 2 + 1];
    }
    const double n = frames ? static_cast<double>(frames) : 1.0;
    return {static_cast<float>(std::sqrt(l / n)), static_cast<float>(std::sqrt(r / n))};
}

std::unique_ptr<AudioEngine> make() {
    std::string error;
    auto a = AudioEngine::create({.null_device = true}, &error);
    REQUIRE_MESSAGE(a, error);
    return a;
}

} // namespace

TEST_CASE("sounds decode from memory; garbage is refused") {
    auto a = make();
    CHECK_FALSE(a->has_device());
    const SoundHandle tone = a->load(sine_wav(0.5f), "tone.wav");
    REQUIRE(tone.valid());
    CHECK(a->info(tone).seconds == doctest::Approx(0.5f).epsilon(0.01));
    CHECK(a->info(tone).channels == 1);
    const std::vector<std::uint8_t> junk(100, 7);
    CHECK_FALSE(a->load(junk, "junk.wav").valid());
    CHECK_FALSE(a->load({}, "empty.wav").valid());
}

TEST_CASE("voices play, end on their own, stop, and buses scale them") {
    auto a = make();
    const SoundHandle tone = a->load(sine_wav(0.25f), "tone.wav");
    CHECK(loudness(*a, 0.05f).first < 1e-4f); // silence before anything plays

    VoiceHandle v = a->play(tone);
    REQUIRE(v.valid());
    CHECK(a->playing(v));
    CHECK(loudness(*a, 0.1f).first > 0.2f);
    loudness(*a, 0.3f); // past the end
    a->update();
    CHECK_FALSE(a->playing(v));
    CHECK(a->voice_count() == 0);

    v = a->play(tone, {.loop = true});
    loudness(*a, 0.5f);
    CHECK(a->playing(v)); // looping goes on
    a->set_bus_volume(AudioBus::Effects, 0.0f);
    loudness(*a, 0.02f); // the volume change ramps in
    CHECK(loudness(*a, 0.1f).first < 1e-3f);
    a->set_bus_volume(AudioBus::Effects, 1.0f);
    CHECK(loudness(*a, 0.1f).first > 0.2f);
    a->stop(v);
    CHECK(a->voice_count() == 0);
    CHECK(loudness(*a, 0.1f).first < 1e-4f);

    a->play(tone, {.bus = AudioBus::Music, .volume = 0.5f});
    const float half = loudness(*a, 0.1f).first;
    CHECK(half > 0.1f);
    CHECK(half < 0.45f);
    a->destroy(tone); // stops its voices
    CHECK(a->voice_count() == 0);
}

TEST_CASE("3D voices pan towards their side and fade with distance") {
    auto a = make();
    const SoundHandle tone = a->load(sine_wav(1.0f), "tone.wav");
    a->set_listener({0, 0, 0}, {0, 0, -1});
    const VoiceHandle right = a->play(tone, {.spatial = true, .position = {5, 0, 0}, .min_distance = 1, .max_distance = 100});
    loudness(*a, 0.05f);
    const auto [l, r] = loudness(*a, 0.1f);
    CHECK(r > l * 1.5f);
    a->stop(right);

    const VoiceHandle near_voice = a->play(tone, {.spatial = true, .position = {0, 0, -1}});
    loudness(*a, 0.05f);
    const float near_level = loudness(*a, 0.1f).first;
    a->set_position(near_voice, {0, 0, -40});
    loudness(*a, 0.05f);
    const float far_level = loudness(*a, 0.1f).first;
    CHECK(far_level < near_level * 0.3f);
}

TEST_CASE("the voice limit holds") {
    std::string error;
    auto a = AudioEngine::create({.null_device = true, .max_voices = 2}, &error);
    REQUIRE(a);
    const SoundHandle tone = a->load(sine_wav(0.5f), "tone.wav");
    CHECK(a->play(tone).valid());
    CHECK(a->play(tone).valid());
    CHECK_FALSE(a->play(tone).valid());
    CHECK(a->voice_count() == 2);
}
