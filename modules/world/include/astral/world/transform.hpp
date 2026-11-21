#pragma once

#include <astral/core/math.hpp>
#include <astral/world/world.hpp>

namespace astral {

/// Local transform, relative to the Parent if there is one.
struct Transform {
    Vec3 position{};
    Quat rotation{};
    Vec3 scale{1, 1, 1};

    [[nodiscard]] Mat4 matrix() const noexcept { return compose(position, rotation, scale); }
};

/// Makes this entity's Transform relative to another entity.
struct Parent {
    Entity entity;
};

/// Final object-to-world matrix, written by update_transforms(). Read it, don't write it.
struct WorldTransform {
    Mat4 matrix = Mat4::identity();
};

/// Computes WorldTransform for every entity with a Transform (adding the component where
/// missing). Parents that no longer exist are ignored; cycles are broken.
void update_transforms(World& world);

} // namespace astral
