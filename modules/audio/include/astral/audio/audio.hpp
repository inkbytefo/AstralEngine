#pragma once
// Sound: decoded clips, voices that play them (2D or positioned in 3D), mixer buses, a listener.
// Pure engine module (only Core): the App connects it to entities (AudioSource, AudioListener).
//
//     auto audio = AudioEngine::create({});
//     SoundHandle hit = audio->load(bytes, "sfx/hit.wav");          // WAV, FLAC, MP3
//     audio->play(hit, {.bus = AudioBus::Effects, .position = p});  // 3D, attenuated by distance
//     audio->set_listener(camera_position, camera_forward, {0, 1, 0});
//     audio->update();                                               // every frame: frees finished voices
//
// With `null_device` nothing is played: the mix is pulled with read() (servers, deterministic tests).

#include <astral/core/handle.hpp>
#include <astral/core/log.hpp>
#include <astral/core/math.hpp>
#include <astral/core/reflect.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace astral {

struct SoundTag;
struct VoiceTag;
using SoundHandle = Handle<SoundTag>; ///< A loaded clip.
using VoiceHandle = Handle<VoiceTag>; ///< One playing instance of a clip.

/// Mixer buses (all feed the master volume).
enum class AudioBus : std::uint8_t { Effects, Music, Ambience, Interface, Count };

struct AudioDesc {
    bool null_device = false;       ///< No output device: mix on demand with read().
    std::uint32_t sample_rate = 48000;
    std::uint32_t channels = 2;
    std::uint32_t max_voices = 256; ///< Further play() calls fail while this many are playing.
    Logger* logger = nullptr;
};

struct PlayDesc {
    AudioBus bus = AudioBus::Effects;
    float volume = 1.0f;
    float pitch = 1.0f;             ///< 2 = an octave up (and twice as fast).
    bool loop = false;
    bool spatial = false;           ///< Positioned in 3D (panned and attenuated relative to the listener).
    Vec3 position{};
    float min_distance = 1.0f;      ///< Full volume up to here (metres)...
    float max_distance = 60.0f;     ///< ...fading out until here.
    float fade_in = 0.0f;           ///< Seconds.
};

struct SoundInfo {
    float seconds = 0.0f;
    std::uint32_t channels = 0;
    std::uint32_t sample_rate = 0;
};

class AudioEngine {
public:
    /// Null when no device could be opened and `null_device` was not requested (`error` says why).
    static std::unique_ptr<AudioEngine> create(const AudioDesc& desc, std::string* error = nullptr);
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /// Decodes an encoded clip (copied; WAV, FLAC, MP3). `stream`: decode while playing (long music)
    /// instead of up front. Invalid handle if the data cannot be decoded.
    SoundHandle load(std::span<const std::uint8_t> encoded, std::string_view name, bool stream = false);
    /// Stops its voices and frees it.
    void destroy(SoundHandle sound);
    [[nodiscard]] SoundInfo info(SoundHandle sound) const;

    /// Starts a voice. Invalid if the sound is invalid or too many voices play.
    VoiceHandle play(SoundHandle sound, const PlayDesc& desc = {});
    /// Stops (after an optional fade); the handle becomes invalid once the voice ends.
    void stop(VoiceHandle voice, float fade_out = 0.0f);
    [[nodiscard]] bool playing(VoiceHandle voice) const;
    void set_volume(VoiceHandle voice, float volume);
    void set_pitch(VoiceHandle voice, float pitch);
    void set_position(VoiceHandle voice, Vec3 position);

    void set_listener(Vec3 position, Vec3 forward, Vec3 up = {0, 1, 0});
    void set_bus_volume(AudioBus bus, float volume);
    [[nodiscard]] float bus_volume(AudioBus bus) const;
    void set_master_volume(float volume);

    /// Main thread, once per frame: releases voices that finished.
    void update();
    [[nodiscard]] std::uint32_t voice_count() const;
    [[nodiscard]] bool has_device() const noexcept;
    [[nodiscard]] std::uint32_t sample_rate() const noexcept;
    [[nodiscard]] std::uint32_t channels() const noexcept;

    /// Null device only: mixes the next `frames` frames (interleaved floats, channels() per frame).
    std::size_t read(std::span<float> out);
    /// Null device only: total frames mixed by read() so far (0 with a real device).
    [[nodiscard]] std::uint64_t mixed_frames() const noexcept;

    struct Impl;

private:
    explicit AudioEngine(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace astral

ASTRAL_REFLECT_ENUM(astral::AudioBus, Effects, Music, Ambience, Interface)
