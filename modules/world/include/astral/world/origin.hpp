#pragma once
// Floating origin (ADR-0021): files keep double world positions; at run time Transform stays float but
// is relative to a movable origin. When the focus (camera/player) gets far from local zero, the origin
// jumps to it and every top-level transform, physics body and render history moves by the same offset.

#include <astral/core/math.hpp>

#include <optional>

namespace astral {

class World;

/// A position in the big (double precision) world.
struct WorldPosition {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// Event payload: local positions moved by -offset; `origin` is the new world position of local zero.
struct OriginShifted {
    Vec3 offset{};
    WorldPosition origin{};
};

class FloatingOrigin {
public:
    /// Shifts when the focus is farther than `threshold` metres from local zero (horizontally);
    /// the new origin is snapped to multiples of `snap` metres (keeps shifts exact in float).
    explicit FloatingOrigin(float threshold = 2000.0f, float snap = 100.0f) noexcept;

    /// Returns the offset to apply (locals move by -offset) when a shift is due; y never shifts.
    std::optional<Vec3> update(Vec3 focus_local) noexcept;

    [[nodiscard]] const WorldPosition& origin() const noexcept { return origin_; }
    [[nodiscard]] WorldPosition to_world(Vec3 local) const noexcept;
    [[nodiscard]] Vec3 to_local(const WorldPosition& world) const noexcept;

private:
    float threshold_;
    float snap_;
    WorldPosition origin_{};
};

/// Moves every Transform without a Parent by -offset (children follow their parents).
void shift_roots(World& world, Vec3 offset);

} // namespace astral
