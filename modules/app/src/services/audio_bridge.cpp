#include "audio_bridge.hpp"

#include <astral/app/components.hpp>
#include <astral/assets/asset_manager.hpp>
#include <astral/core/log.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

namespace astral {

namespace {

Vec3 entity_position(World& world, Entity e) noexcept {
    const WorldTransform* w = world.get<const WorldTransform>(e);
    return w ? Vec3{w->matrix[3].x, w->matrix[3].y, w->matrix[3].z} : Vec3{};
}

} // namespace

AudioBridge::AudioBridge(World& world, AssetManager& assets, Logger& log, bool enable_device)
    : world_(world), assets_(assets), log_(log) {
    std::string error;
    if (enable_device) audio_ = AudioEngine::create({.logger = &log_}, &error);
    if (!audio_) {
        if (enable_device) log_.log(LogLevel::Warn, error + "; sound is mixed silently");
        audio_ = AudioEngine::create({.null_device = true, .logger = &log_}, &error);
    }
}

AudioBridge::~AudioBridge() = default;

SoundHandle AudioBridge::sound(std::string_view path) {
    if (const auto it = sounds_.find(path); it != sounds_.end()) return it->second;
    auto [it, inserted] = sounds_.try_emplace(std::string(path));
    if (!inserted) return it->second;
    std::string error;
    if (const auto bytes = assets_.read(path, &error)) {
        it->second = audio_->load(*bytes, path);
        if (!it->second.valid()) {
            log_.log(LogLevel::Error, "sound " + std::string(path) + ": cannot decode (WAV, FLAC or MP3)");
        }
    } else {
        log_.log(LogLevel::Error, "sound: " + error);
    }
    return it->second;
}

void AudioBridge::update_listener() {
    // Listener: an AudioListener entity, else the first active camera.
    bool listener = false;
    auto listen_from = [&](const Mat4& m) {
        const Vec3 forward = normalize(Vec3{-m[2].x, -m[2].y, -m[2].z});
        const Vec3 up = normalize(Vec3{m[1].x, m[1].y, m[1].z});
        audio_->set_listener({m[3].x, m[3].y, m[3].z}, forward, up);
        listener = true;
    };
    world_.each<const AudioListener, const WorldTransform>([&](const AudioListener&, const WorldTransform& w) {
        if (!listener) listen_from(w.matrix);
    });
    if (!listener) {
        world_.each<const Camera, const WorldTransform>([&](const Camera& c, const WorldTransform& w) {
            if (!listener && c.active) listen_from(w.matrix);
        });
    }
}

void AudioBridge::sync() {
    update_listener();

    gone_voices_.clear();
    world_.query<const AudioVoice>().without<AudioSource>().each(
        [&](Entity e, const AudioVoice&) { gone_voices_.push_back(e); });
    for (Entity e : gone_voices_) {
        if (const AudioVoice* v = world_.get<AudioVoice>(e); v && v->voice.valid()) {
            audio_->stop(v->voice, 0.05f);
        }
        world_.remove<AudioVoice>(e);
    }

    new_voices_.clear();
    world_.each<AudioSource>([&](Entity e, AudioSource& src) {
        AudioVoice* v = world_.get<AudioVoice>(e);
        const bool alive = v && v->voice.valid() && audio_->playing(v->voice);
        if (v && (v->sound != src.sound || !src.playing)) {
            if (v->voice.valid()) {
                audio_->stop(v->voice, 0.05f);
                // Clear handle immediately so an immediate restart on the next frame is not mistaken for an ended voice.
                v->voice = {};
            }
            if (!src.playing) return;
        }
        if (alive && v->sound == src.sound) {
            audio_->set_volume(v->voice, src.volume);
            audio_->set_pitch(v->voice, src.pitch);
            if (src.spatial) audio_->set_position(v->voice, entity_position(world_, e));
            return;
        }
        if (v && v->sound == src.sound && v->voice.valid() && !alive) { // a one-shot ended naturally
            src.playing = false;
            v->voice = {};
            return;
        }
        if (!src.playing || src.sound.empty()) return;

        const SoundHandle handle = sound(src.sound);
        const VoiceHandle voice = audio_->play(
            handle, {.bus = src.bus,
                     .volume = src.volume,
                     .pitch = src.pitch,
                     .loop = src.loop,
                     .spatial = src.spatial,
                     .position = entity_position(world_, e),
                     .min_distance = src.min_distance,
                     .max_distance = src.max_distance});
        if (v) {
            v->voice = voice;
            v->sound = src.sound;
        } else {
            new_voices_.emplace_back(e, AudioVoice{voice, src.sound});
        }
    });
    for (auto& [e, voice] : new_voices_) world_.add(e, std::move(voice));
    audio_->update();
}

} // namespace astral
