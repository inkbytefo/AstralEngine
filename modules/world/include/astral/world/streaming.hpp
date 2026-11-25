#pragma once
// Cell streaming decisions (ADR-0021): which cells of a map should be in the world, given where the
// streaming sources are. The actual reading and spawning is done by a CellLoader (the App reads cells
// on the JobSystem and spawns them on the main thread); here everything is single-threaded and testable.

#include <astral/world/map.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace astral {

/// A streaming source position on the ground plane (camera, player, vehicle look-ahead...).
struct StreamingPoint {
    double x = 0.0;
    double z = 0.0;
};

/// Does the work for CellStreamer. begin_load may start asynchronous work; ready() polls it;
/// attach() puts the loaded cell into the world (main thread) and returns false when the load
/// failed (nothing was attached; the streamer retries later); detach() removes it; cancel()
/// abandons a load that is no longer wanted (whether or not it finished).
class CellLoader {
public:
    virtual ~CellLoader() = default;
    virtual void begin_load(CellKey cell) = 0;
    virtual bool ready(CellKey cell) = 0;
    virtual bool attach(CellKey cell) = 0;
    virtual void detach(CellKey cell) = 0;
    virtual void cancel(CellKey cell) = 0;
};

struct StreamingStats {
    std::uint32_t attached = 0;             ///< Cells in the world.
    std::uint32_t loading = 0;              ///< Started, not ready yet.
    std::uint32_t waiting = 0;              ///< Ready, waiting for the attach budget.
    std::uint32_t attached_last_update = 0;
    std::uint32_t detached_last_update = 0;
    std::uint32_t failed_last_update = 0;   ///< attach() reported a failed load; retried after a back-off.
};

class CellStreamer {
public:
    /// Cells beyond `load_range * (1 + hysteresis)` of every source are unloaded; new loads start
    /// within `load_range` (so cells near the edge do not load and unload back and forth).
    CellStreamer(MapDesc map, std::vector<CellKey> cells, CellLoader& loader, double hysteresis = 0.1);
    ~CellStreamer();
    CellStreamer(const CellStreamer&) = delete;
    CellStreamer& operator=(const CellStreamer&) = delete;

    /// Starts/cancels loads, detaches far cells, attaches at most `max_attach` ready cells (nearest first).
    void update(std::span<const StreamingPoint> sources, std::uint32_t max_attach);
    /// Detaches every attached cell and cancels every load (closing the map).
    void unload_all();

    [[nodiscard]] bool is_attached(CellKey cell) const noexcept;
    [[nodiscard]] const StreamingStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const MapDesc& map() const noexcept { return map_; }

private:
    enum class State : std::uint8_t { Unloaded, Loading, Attached };
    struct Entry {
        CellKey key;
        State state = State::Unloaded;
        double distance = 0.0; ///< To the nearest source, this update.
        std::uint32_t failures = 0; ///< Consecutive failed attaches (reset on success).
        std::uint32_t cooldown = 0; ///< Updates to wait before loading again after a failure.
    };

    MapDesc map_;
    std::vector<Entry> cells_;
    CellLoader& loader_;
    double hysteresis_;
    StreamingStats stats_;
};

} // namespace astral
