#pragma once

#include <astral/audio/audio.hpp>

#include <string_view>

namespace astral {

/// Cached sound loading bound to an App's Context.
class AudioService {
public:
    virtual ~AudioService() = default;
    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    /// Loads a sound by asset path once (from the pack or the asset folder) and returns it; invalid
    /// (and logged once) if it cannot be read or decoded.
    /// ctx.audio->play(ctx.audio_service().sound("sfx/hit.wav")).
    virtual SoundHandle sound(std::string_view path) = 0;

protected:
    AudioService() = default;
};

} // namespace astral
