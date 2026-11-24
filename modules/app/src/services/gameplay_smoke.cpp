#include <astral/app/gameplay_smoke.hpp>
#include <astral/app/physics_service.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace astral {
namespace {

bool finite(Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

constexpr std::array<std::array<int, 3>, 6> neighbors{{
    {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
}};

std::size_t index_of(int x, int y, int z, unsigned side) noexcept {
    return (static_cast<std::size_t>(z) * side + static_cast<unsigned>(y)) * side + static_cast<unsigned>(x);
}

} // namespace

GameplaySmoke::GameplaySmoke(GameplaySmokeDesc desc)
    : desc_(desc), side_(2 * std::clamp(desc.radius_cells, 1u, 12u) + 1), occupied_(side_ * side_ * side_, 0) {
    if (!std::isfinite(desc_.cell_size_m) || desc_.cell_size_m < 0.01f) desc_.cell_size_m = 0.25f;
    if (!std::isfinite(desc_.extinction_per_m) || desc_.extinction_per_m < 0) desc_.extinction_per_m = 0;
    desc_.spread_interval_steps = std::max(1u, desc_.spread_interval_steps);
    occupied_[occupied_.size() / 2] = 1;
}

void GameplaySmoke::step(const PhysicsService& physics, Vec3 centre) {
    if (tick_ >= static_cast<std::uint64_t>(desc_.spread_steps) + desc_.fade_steps) return;
    ++tick_;
    if (!restore_at_.empty()) {
        bool pending = false;
        for (std::size_t i = 0; i < restore_at_.size(); ++i) {
            if (restore_at_[i] == 0) continue;
            if (restore_at_[i] <= tick_) {
                occupied_[i] = 1;
                restore_at_[i] = 0;
            } else {
                pending = true;
            }
        }
        if (!pending) restore_at_.clear();
    }
    if (!finite(centre) || tick_ > desc_.spread_steps || tick_ % desc_.spread_interval_steps != 0) return;

    const int side = static_cast<int>(side_);
    const int middle = side / 2;
    std::vector<RayQuery> rays;
    std::vector<std::size_t> targets;
    for (int z = 0; z < side; ++z) {
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                if (!occupied_[index_of(x, y, z, side_)]) continue;
                const Vec3 origin = centre + Vec3{static_cast<float>(x - middle), static_cast<float>(y - middle),
                                                  static_cast<float>(z - middle)} * desc_.cell_size_m;
                for (const auto& n : neighbors) {
                    const int nx = x + n[0], ny = y + n[1], nz = z + n[2];
                    if (nx < 0 || nx >= side || ny < 0 || ny >= side || nz < 0 || nz >= side) continue;
                    const auto target = index_of(nx, ny, nz, side_);
                    if (occupied_[target] || (!restore_at_.empty() && restore_at_[target] != 0)) continue;
                    rays.push_back({origin, {static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])},
                                    desc_.cell_size_m, {.layers = desc_.obstacle_layers}});
                    targets.push_back(target);
                }
            }
        }
    }
    if (rays.empty()) return;
    // Apply only after the whole batch completes: at most one cell of propagation per spread tick.
    std::vector<std::optional<RayHit>> hits(rays.size());
    physics.raycast_batch(rays, hits);
    for (std::size_t i = 0; i < hits.size(); ++i) {
        if (!hits[i]) occupied_[targets[i]] = 1;
    }
}

void GameplaySmoke::disperse(Vec3 centre, Vec3 from, Vec3 to, float radius_m, unsigned hold_steps) {
    if (!finite(centre) || !finite(from) || !finite(to) || !std::isfinite(radius_m) ||
        radius_m <= 0 || hold_steps == 0 || density_scale() <= 0) return;
    const Vec3 start = from - centre;
    const Vec3 end = to - centre;
    const Vec3 segment = end - start;
    const float segment_sq = dot(segment, segment);
    const float radius_sq = radius_m * radius_m;
    const float half = static_cast<float>(side_ / 2) * desc_.cell_size_m;
    if (!std::isfinite(segment_sq) || !std::isfinite(radius_sq) || !std::isfinite(half)) return;
    const float reach = half + radius_m;
    if (std::min(start.x, end.x) > reach || std::max(start.x, end.x) < -reach ||
        std::min(start.y, end.y) > reach || std::max(start.y, end.y) < -reach ||
        std::min(start.z, end.z) > reach || std::max(start.z, end.z) < -reach) return;

    const auto restore = std::min(tick_ + hold_steps, static_cast<std::uint64_t>(desc_.spread_steps) + desc_.fade_steps);
    const int side = static_cast<int>(side_);
    const int middle = side / 2;
    for (int z = 0; z < side; ++z) {
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                const auto i = index_of(x, y, z, side_);
                if (!occupied_[i] && (restore_at_.empty() || restore_at_[i] == 0)) continue;
                const Vec3 offset = Vec3{static_cast<float>(x - middle), static_cast<float>(y - middle),
                                         static_cast<float>(z - middle)} * desc_.cell_size_m - start;
                const float t = segment_sq > 0 ? std::clamp(dot(offset, segment) / segment_sq, 0.0f, 1.0f) : 0.0f;
                const Vec3 radial = offset - segment * t;
                if (!(dot(radial, radial) <= radius_sq)) continue;
                if (restore_at_.empty()) restore_at_.resize(occupied_.size());
                occupied_[i] = 0;
                restore_at_[i] = std::max(restore_at_[i], restore);
            }
        }
    }
}

float GameplaySmoke::density_scale() const noexcept {
    if (tick_ < desc_.spread_steps) return 1.0f;
    if (desc_.fade_steps == 0) return 0;
    return 1.0f - static_cast<float>(tick_ - desc_.spread_steps) / static_cast<float>(desc_.fade_steps);
}

float GameplaySmoke::optical_depth(Vec3 centre, Vec3 from, Vec3 to) const noexcept {
    const Vec3 delta = to - from;
    const float distance = length(delta);
    const float density = density_scale() * desc_.extinction_per_m;
    if (!finite(centre) || !finite(from) || !std::isfinite(distance) || distance <= 0 || density <= 0) return 0;

    const double half = static_cast<double>(side_) * desc_.cell_size_m * 0.5;
    const double size = desc_.cell_size_m;
    const std::array<double, 3> start{(from.x - static_cast<double>(centre.x) + half) / size,
        (from.y - static_cast<double>(centre.y) + half) / size, (from.z - static_cast<double>(centre.z) + half) / size};
    const std::array<double, 3> dir{delta.x / size, delta.y / size, delta.z / size};
    double enter = 0, exit = 1;
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (dir[axis] == 0) {
            if (start[axis] < 0 || start[axis] >= side_) return 0;
            continue;
        }
        const double a = -start[axis] / dir[axis];
        const double b = (side_ - start[axis]) / dir[axis];
        enter = std::max(enter, std::min(a, b));
        exit = std::min(exit, std::max(a, b));
    }
    if (enter >= exit) return 0;

    std::array<int, 3> cell{}, direction{};
    std::array<double, 3> edge{};
    for (unsigned axis = 0; axis < 3; ++axis) {
        const double at = std::clamp(start[axis] + enter * dir[axis], 0.0, static_cast<double>(side_));
        direction[axis] = dir[axis] < 0 ? -1 : 1;
        const double first = direction[axis] < 0 ? std::ceil(at) - 1 : std::floor(at);
        cell[axis] = std::clamp(static_cast<int>(first), 0, static_cast<int>(side_) - 1);
        edge[axis] = dir[axis] == 0 ? std::numeric_limits<double>::infinity() :
            (cell[axis] + (direction[axis] > 0 ? 1 : 0) - start[axis]) / dir[axis];
    }

    double filled = 0;
    while (enter < exit) {
        const double next = std::min({exit, edge[0], edge[1], edge[2]});
        if (occupied_[index_of(cell[0], cell[1], cell[2], side_)]) filled += std::max(0.0, next - enter);
        if (next >= exit) break;
        for (unsigned axis = 0; axis < 3; ++axis) {
            if (edge[axis] > next) continue;
            cell[axis] += direction[axis];
            if (cell[axis] < 0 || cell[axis] >= static_cast<int>(side_)) return static_cast<float>(filled * distance * density);
            edge[axis] += 1.0 / std::abs(dir[axis]);
        }
        enter = next;
    }
    return static_cast<float>(filled * distance * density);
}

float GameplaySmoke::transmittance(Vec3 centre, Vec3 from, Vec3 to) const noexcept {
    return std::exp(-optical_depth(centre, from, to));
}

} // namespace astral
