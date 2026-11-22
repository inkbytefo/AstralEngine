#include <astral/world/streaming.hpp>

#include <algorithm>
#include <limits>

namespace astral {

CellStreamer::CellStreamer(MapDesc map, std::vector<CellKey> cells, CellLoader& loader, double hysteresis)
    : map_(std::move(map)), loader_(loader), hysteresis_(hysteresis) {
    cells_.reserve(cells.size());
    for (CellKey key : cells) cells_.push_back({key, State::Unloaded, 0.0});
}

CellStreamer::~CellStreamer() = default;

void CellStreamer::update(std::span<const StreamingPoint> sources, std::uint32_t max_attach) {
    stats_.attached_last_update = 0;
    stats_.detached_last_update = 0;
    std::vector<Entry*> ready;
    for (Entry& cell : cells_) {
        cell.distance = std::numeric_limits<double>::infinity();
        for (const StreamingPoint& s : sources) {
            cell.distance = std::min(cell.distance, cell_distance(map_, cell.key, s.x, s.z));
        }
        const double range = cell.key.grid < map_.grids.size() ? map_.grids[cell.key.grid].load_range : 0.0;
        const bool wanted = cell.distance <= range;
        const bool keep = cell.distance <= range * (1.0 + hysteresis_);
        switch (cell.state) {
        case State::Unloaded:
            if (wanted) {
                loader_.begin_load(cell.key);
                cell.state = State::Loading;
            }
            break;
        case State::Loading:
            if (!keep) {
                loader_.cancel(cell.key);
                cell.state = State::Unloaded;
            } else if (loader_.ready(cell.key)) {
                ready.push_back(&cell);
            }
            break;
        case State::Attached:
            if (!keep) {
                loader_.detach(cell.key);
                cell.state = State::Unloaded;
                ++stats_.detached_last_update;
            }
            break;
        }
    }

    // Nearest first (ties: stable cell order) so the space around the player fills in first.
    std::stable_sort(ready.begin(), ready.end(), [](const Entry* a, const Entry* b) { return a->distance < b->distance; });
    const std::size_t count = std::min<std::size_t>(ready.size(), max_attach);
    for (std::size_t i = 0; i < count; ++i) {
        loader_.attach(ready[i]->key);
        ready[i]->state = State::Attached;
        ++stats_.attached_last_update;
    }

    stats_.attached = 0;
    stats_.loading = 0;
    for (const Entry& cell : cells_) {
        if (cell.state == State::Attached) ++stats_.attached;
        if (cell.state == State::Loading) ++stats_.loading;
    }
    stats_.waiting = static_cast<std::uint32_t>(ready.size() - count);
    stats_.loading -= stats_.waiting;
}

void CellStreamer::unload_all() {
    for (Entry& cell : cells_) {
        if (cell.state == State::Attached) loader_.detach(cell.key);
        if (cell.state == State::Loading) loader_.cancel(cell.key);
        cell.state = State::Unloaded;
    }
    stats_ = {};
}

bool CellStreamer::is_attached(CellKey cell) const noexcept {
    return std::any_of(cells_.begin(), cells_.end(),
                       [&](const Entry& e) { return e.key == cell && e.state == State::Attached; });
}

} // namespace astral
