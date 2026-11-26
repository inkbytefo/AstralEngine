#pragma once
// Internal to Astral::App: bridges AudioSource / AudioListener components to AudioEngine and caches loaded sounds.

#include "../string_hash.hpp"

#include <astral/app/audio_service.hpp>
#include <astral/audio/audio.hpp>
#include <astral/world/world.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astral {

class AssetManager;
class Logger;
class World;

/// Internal: tracks the active audio voice attached to an AudioSource entity.
struct AudioVoice {
    VoiceHandle voice;
    std::string sound;
};

class AudioBridge final : public AudioService {
public:
    AudioBridge(World& world, AssetManager& assets, Logger& log, bool enable_device);
    ~AudioBridge() override;
    AudioBridge(const AudioBridge&) = delete;
    AudioBridge& operator=(const AudioBridge&) = delete;

    [[nodiscard]] AudioEngine* engine() noexcept { return audio_.get(); }
    SoundHandle sound(std::string_view path) override;
    /// Starts/stops voices for AudioSource entities. `delta` (seconds) advances the silent mixer of a
    /// device-less engine, so sounds end in headless runs too.
    void sync(float delta = 0.0f);

private:
    void update_listener();
    void advance_silent_mixer(float delta);

    World& world_;
    AssetManager& assets_;
    Logger& log_;
    std::unique_ptr<AudioEngine> audio_;
    std::unordered_map<std::string, SoundHandle, StringHash, std::equal_to<>> sounds_;
    std::vector<Entity> gone_voices_;
    std::vector<std::pair<Entity, AudioVoice>> new_voices_;
    /// Voices started for entities, as of the end of the last sync: a voice whose entity (or
    /// AudioVoice component) disappeared since is stopped here, since no query can find it anymore.
    std::vector<std::pair<Entity, VoiceHandle>> owned_voices_;
    /// Frames the silent mixer should have produced by now (game clock), and its scratch output.
    double silent_clock_frames_ = 0.0;
    std::vector<float> silent_scratch_;
};

} // namespace astral
