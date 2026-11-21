#include <astral/audio/audio.hpp>

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <vector>

namespace astral {

namespace {
struct SoundRecord {
    std::string key;                     ///< Unique name in miniaudio's resource manager.
    std::vector<std::uint8_t> encoded;   ///< Registered with the resource manager (must outlive it).
    std::unique_ptr<ma_sound> prototype; ///< Never played: keeps the decoded data cached.
    bool stream = false;
    SoundInfo info;
};
struct VoiceRecord {
    std::unique_ptr<ma_sound> sound;
    SoundHandle source;
};
} // namespace

struct AudioEngine::Impl {
    AudioDesc desc;
    ma_engine engine{};
    std::array<ma_sound_group, static_cast<std::size_t>(AudioBus::Count)> buses{};
    std::array<float, static_cast<std::size_t>(AudioBus::Count)> bus_volumes{};
    HandlePool<SoundRecord, SoundTag> sounds;
    HandlePool<VoiceRecord, VoiceTag> voices;
    std::uint32_t voice_total = 0;
    std::uint64_t next_key = 0;
    bool device = false;

    void warn(const std::string& message) const {
        if (desc.logger) desc.logger->log(LogLevel::Warn, "audio: " + message);
    }
    void release(VoiceHandle h) {
        VoiceRecord* v = voices.get(h);
        if (!v) return;
        ma_sound_uninit(v->sound.get());
        voices.destroy(h);
        --voice_total;
    }
};

std::unique_ptr<AudioEngine> AudioEngine::create(const AudioDesc& desc, std::string* error) {
    auto impl = std::make_unique<Impl>();
    impl->desc = desc;
    ma_engine_config config = ma_engine_config_init();
    config.noDevice = desc.null_device ? MA_TRUE : MA_FALSE;
    config.channels = desc.channels;
    config.sampleRate = desc.sample_rate;
    if (ma_engine_init(&config, &impl->engine) != MA_SUCCESS) {
        if (error) *error = desc.null_device ? "audio: could not create the mixer" : "audio: no usable output device";
        return nullptr;
    }
    impl->device = !desc.null_device;
    for (std::size_t i = 0; i < impl->buses.size(); ++i) {
        ma_sound_group_init(&impl->engine, 0, nullptr, &impl->buses[i]);
        impl->bus_volumes[i] = 1.0f;
    }
    return std::unique_ptr<AudioEngine>(new AudioEngine(std::move(impl)));
}

AudioEngine::AudioEngine(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

AudioEngine::~AudioEngine() {
    auto& m = *impl_;
    m.voices.for_each([&](VoiceHandle, VoiceRecord& v) { ma_sound_uninit(v.sound.get()); });
    ma_resource_manager* rm = ma_engine_get_resource_manager(&m.engine);
    m.sounds.for_each([&](SoundHandle, SoundRecord& s) {
        ma_sound_uninit(s.prototype.get());
        ma_resource_manager_unregister_data(rm, s.key.c_str());
    });
    for (auto& g : m.buses) ma_sound_group_uninit(&g);
    ma_engine_uninit(&m.engine);
}

SoundHandle AudioEngine::load(std::span<const std::uint8_t> encoded, std::string_view name, bool stream) {
    auto& m = *impl_;
    SoundRecord record;
    record.key = "astral#" + std::to_string(m.next_key++) + "/" + std::string(name);
    record.encoded.assign(encoded.begin(), encoded.end());
    record.stream = stream;
    record.prototype = std::make_unique<ma_sound>();
    ma_resource_manager* rm = ma_engine_get_resource_manager(&m.engine);
    if (record.encoded.empty() ||
        ma_resource_manager_register_encoded_data(rm, record.key.c_str(), record.encoded.data(), record.encoded.size()) != MA_SUCCESS) {
        m.warn(std::string(name) + ": cannot register the data");
        return {};
    }
    const ma_uint32 flags = (stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE) | MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (ma_sound_init_from_file(&m.engine, record.key.c_str(), flags, nullptr, nullptr, record.prototype.get()) != MA_SUCCESS) {
        ma_resource_manager_unregister_data(rm, record.key.c_str());
        m.warn(std::string(name) + ": not a WAV, FLAC or MP3 file");
        return {};
    }
    ma_format format;
    ma_uint32 channels = 0, rate = 0;
    ma_sound_get_data_format(record.prototype.get(), &format, &channels, &rate, nullptr, 0);
    ma_uint64 frames = 0;
    ma_sound_get_length_in_pcm_frames(record.prototype.get(), &frames);
    record.info = {rate ? static_cast<float>(frames) / static_cast<float>(rate) : 0.0f, channels, rate};
    return m.sounds.create(std::move(record));
}

void AudioEngine::destroy(SoundHandle sound) {
    auto& m = *impl_;
    SoundRecord* s = m.sounds.get(sound);
    if (!s) return;
    std::vector<VoiceHandle> stale;
    m.voices.for_each([&](VoiceHandle h, VoiceRecord& v) {
        if (v.source == sound) stale.push_back(h);
    });
    for (VoiceHandle h : stale) m.release(h);
    ma_sound_uninit(s->prototype.get());
    ma_resource_manager_unregister_data(ma_engine_get_resource_manager(&m.engine), s->key.c_str());
    m.sounds.destroy(sound);
}

SoundInfo AudioEngine::info(SoundHandle sound) const {
    const SoundRecord* s = impl_->sounds.get(sound);
    return s ? s->info : SoundInfo{};
}

VoiceHandle AudioEngine::play(SoundHandle sound, const PlayDesc& d) {
    auto& m = *impl_;
    SoundRecord* s = m.sounds.get(sound);
    if (!s) return {};
    if (m.voice_total >= m.desc.max_voices) {
        m.warn("too many voices playing; " + s->key + " skipped");
        return {};
    }
    VoiceRecord v;
    v.sound = std::make_unique<ma_sound>();
    v.source = sound;
    ma_uint32 flags = s->stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
    if (!d.spatial) flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    const std::size_t bus = std::min<std::size_t>(static_cast<std::size_t>(d.bus), m.buses.size() - 1);
    if (ma_sound_init_from_file(&m.engine, s->key.c_str(), flags, &m.buses[bus], nullptr, v.sound.get()) != MA_SUCCESS) return {};
    ma_sound* snd = v.sound.get();
    ma_sound_set_volume(snd, d.volume);
    ma_sound_set_pitch(snd, d.pitch);
    ma_sound_set_looping(snd, d.loop ? MA_TRUE : MA_FALSE);
    if (d.spatial) {
        ma_sound_set_position(snd, d.position.x, d.position.y, d.position.z);
        ma_sound_set_min_distance(snd, d.min_distance);
        ma_sound_set_max_distance(snd, d.max_distance);
        ma_sound_set_doppler_factor(snd, 0.0f); // positions are set per frame, not velocities
    }
    if (d.fade_in > 0.0f) ma_sound_set_fade_in_milliseconds(snd, 0.0f, d.volume, static_cast<ma_uint64>(d.fade_in * 1000.0f));
    ma_sound_start(snd);
    ++m.voice_total;
    return m.voices.create(std::move(v));
}

void AudioEngine::stop(VoiceHandle voice, float fade_out) {
    auto& m = *impl_;
    VoiceRecord* v = m.voices.get(voice);
    if (!v) return;
    if (fade_out > 0.0f) ma_sound_stop_with_fade_in_milliseconds(v->sound.get(), static_cast<ma_uint64>(fade_out * 1000.0f));
    else m.release(voice);
}

bool AudioEngine::playing(VoiceHandle voice) const {
    const VoiceRecord* v = impl_->voices.get(voice);
    return v && ma_sound_is_playing(v->sound.get());
}

void AudioEngine::set_volume(VoiceHandle voice, float volume) {
    if (VoiceRecord* v = impl_->voices.get(voice)) ma_sound_set_volume(v->sound.get(), volume);
}
void AudioEngine::set_pitch(VoiceHandle voice, float pitch) {
    if (VoiceRecord* v = impl_->voices.get(voice)) ma_sound_set_pitch(v->sound.get(), pitch);
}
void AudioEngine::set_position(VoiceHandle voice, Vec3 p) {
    if (VoiceRecord* v = impl_->voices.get(voice)) ma_sound_set_position(v->sound.get(), p.x, p.y, p.z);
}

void AudioEngine::set_listener(Vec3 p, Vec3 forward, Vec3 up) {
    ma_engine_listener_set_position(&impl_->engine, 0, p.x, p.y, p.z);
    ma_engine_listener_set_direction(&impl_->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl_->engine, 0, up.x, up.y, up.z);
}

void AudioEngine::set_bus_volume(AudioBus bus, float volume) {
    const auto i = static_cast<std::size_t>(bus);
    if (i >= impl_->buses.size()) return;
    impl_->bus_volumes[i] = volume;
    ma_sound_group_set_volume(&impl_->buses[i], volume);
}

float AudioEngine::bus_volume(AudioBus bus) const {
    const auto i = static_cast<std::size_t>(bus);
    return i < impl_->bus_volumes.size() ? impl_->bus_volumes[i] : 0.0f;
}

void AudioEngine::set_master_volume(float volume) { ma_engine_set_volume(&impl_->engine, volume); }

void AudioEngine::update() {
    auto& m = *impl_;
    std::vector<VoiceHandle> done;
    m.voices.for_each([&](VoiceHandle h, VoiceRecord& v) {
        if (!ma_sound_is_playing(v.sound.get())) done.push_back(h);
    });
    for (VoiceHandle h : done) m.release(h);
}

std::uint32_t AudioEngine::voice_count() const { return impl_->voice_total; }
bool AudioEngine::has_device() const noexcept { return impl_->device; }
std::uint32_t AudioEngine::sample_rate() const noexcept { return ma_engine_get_sample_rate(&impl_->engine); }
std::uint32_t AudioEngine::channels() const noexcept { return ma_engine_get_channels(&impl_->engine); }

std::size_t AudioEngine::read(std::span<float> out) {
    auto& m = *impl_;
    if (m.device || out.empty()) return 0;
    const ma_uint64 frames = out.size() / channels();
    ma_uint64 read = 0;
    ma_engine_read_pcm_frames(&m.engine, out.data(), frames, &read);
    return static_cast<std::size_t>(read);
}

} // namespace astral
