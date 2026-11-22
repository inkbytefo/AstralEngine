#pragma once
// The cubes: a Spin (C++ system: it touches every cube every frame, so it runs on all worker threads)
// and a Bob (Luau rule, assets/features/cubes/bob.luau).

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace cubes {

struct Spin {
    astral::Vec3 axis{0, 1, 0};
    float speed = 1.0f; // radians per second
};

struct Bob {
    float height = 0.5f;
    float phase = 0.0f;
    float base_y = 0.0f;
};

void install_cubes(astral::FeatureContext& feature);

/// A grid x grid field of coloured cubes on a ground plane.
void spawn_grid(astral::Context& ctx, int grid);

} // namespace cubes

ASTRAL_REFLECT(cubes::Spin, axis, speed)
ASTRAL_REFLECT(cubes::Bob, height, phase, base_y)
