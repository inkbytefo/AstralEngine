#include <astral/world/origin.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <cmath>
#include <vector>

namespace astral {

FloatingOrigin::FloatingOrigin(float threshold, float snap) noexcept : threshold_(threshold), snap_(snap > 0.0f ? snap : 1.0f) {}

std::optional<Vec3> FloatingOrigin::update(Vec3 focus_local) noexcept {
    if (std::sqrt(focus_local.x * focus_local.x + focus_local.z * focus_local.z) <= threshold_) return std::nullopt;
    const Vec3 offset{std::round(focus_local.x / snap_) * snap_, 0.0f, std::round(focus_local.z / snap_) * snap_};
    origin_.x += static_cast<double>(offset.x);
    origin_.z += static_cast<double>(offset.z);
    return offset;
}

WorldPosition FloatingOrigin::to_world(Vec3 local) const noexcept {
    return {origin_.x + static_cast<double>(local.x), origin_.y + static_cast<double>(local.y),
            origin_.z + static_cast<double>(local.z)};
}

Vec3 FloatingOrigin::to_local(const WorldPosition& world) const noexcept {
    return {static_cast<float>(world.x - origin_.x), static_cast<float>(world.y - origin_.y),
            static_cast<float>(world.z - origin_.z)};
}

void shift_roots(World& world, Vec3 offset) {
    std::vector<Entity> roots;
    world.each<const Transform>([&](Entity e, const Transform&) {
        if (!world.has<Parent>(e)) roots.push_back(e);
    });
    for (Entity e : roots) {
        Transform* t = world.get<Transform>(e);
        t->position = {t->position.x - offset.x, t->position.y - offset.y, t->position.z - offset.z};
    }
}

} // namespace astral
