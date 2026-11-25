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
    void sync();

private:
    void update_listener();

    World& world_;
    AssetManager& assets_;
    Logger& log_;
    std::unique_ptr<AudioEngine> audio_;
    std::unordered_map<std::string, SoundHandle, StringHash, std::equal_to<>> sounds_;
    std::vector<Entity> gone_voices_;
    std::vector<std::pair<Entity, AudioVoice>> new_voices_;
};

} // namespace astral
