#pragma once

#include <astral/world/map_state.hpp>
#include <astral/world/origin.hpp>
#include <astral/world/streaming.hpp>

#include <string_view>

namespace astral {

/// Open-world map streaming, data layers, and floating origin bound to an App's Context.
class MapService {
public:
    virtual ~MapService() = default;
    MapService(const MapService&) = delete;
    MapService& operator=(const MapService&) = delete;

    /// Opens a map (.world, ADR-0021) in place of the current one: its always-loaded entities spawn now,
    /// its cells stream in and out around StreamingSource entities (else the active camera), and the
    /// floating origin follows the first source. Works with source maps (development) and cooked ones.
    /// False (logged) if the map cannot be read.
    virtual bool open_world(std::string_view path) = 0;
    /// Removes the open map's entities and forgets it.
    virtual void close_world() = 0;
    /// Blocks until every cell in range is loaded and spawned (loading screens, tests).
    virtual void finish_world_loading() = 0;
    /// The open map's data layers (set("night", true)); null without an open map.
    [[nodiscard]] virtual MapLayers* world_layers() noexcept = 0;
    /// Cell counts of the open map; null without one.
    [[nodiscard]] virtual const StreamingStats* world_streaming() const noexcept = 0;
    /// World position of local zero. Transform positions are relative to it; OriginShifted events tell
    /// when it moves (only while a map is open).
    [[nodiscard]] virtual WorldPosition world_origin() const noexcept = 0;

protected:
    MapService() = default;
};

} // namespace astral
