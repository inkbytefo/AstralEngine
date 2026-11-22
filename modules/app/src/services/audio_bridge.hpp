#pragma once
// Internal to Astral::App: bridges AudioSource / AudioListener components to AudioEngine and caches loaded sounds.

#include <astral/audio/audio.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace astral {

class AssetManager;
class Logger;
class World;

class AudioBridge {
public:
    AudioBridge(World& world, AssetManager& assets, Logger& log, bool enable_device);
    ~AudioBridge();
    AudioBridge(const AudioBridge&) = delete;
    AudioBridge& operator=(const AudioBridge&) = delete;

    [[nodiscard]] AudioEngine* engine() noexcept { return audio_.get(); }
    SoundHandle sound(std::string_view path);
    void sync();

private:
    void update_listener();

    World& world_;
    AssetManager& assets_;
    Logger& log_;
    std::unique_ptr<AudioEngine> audio_;
    std::unordered_map<std::string, SoundHandle> sounds_;
};

} // namespace astral
