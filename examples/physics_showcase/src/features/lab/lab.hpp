#pragma once
// The joints station's data: motors driven from Luau, a batched raycast sensor array, and the counters the HUD shows.
// The rules are Luau (assets/features/lab/lab.luau).

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace lab {

/// Drives the joint motor on the same entity: target position = sin(time * frequency * 2pi) * amplitude.
struct MotorOscillator {
    float frequency = 0.6f;
    float amplitude = 1.0f;
};

/// Casts `rays` rays straight down in one batch, starting `height` above the entity, `spacing` apart along x,
/// the last one `far_offset` away along -z (usually off the map, so it misses).
struct SensorArray {
    float height = 12.0f;
    float spacing = 6.0f;
    float reach = 25.0f;
    float far_offset = 44.0f;
};

struct LabStats {
    int broken_joints = 0;
    int detached_joints = 0;
    int batch_ray_hits = 0;
    int motor_updates = 0;
};

void install_lab(astral::FeatureContext& feature);

} // namespace lab

ASTRAL_REFLECT(lab::MotorOscillator, frequency, amplitude)
ASTRAL_REFLECT(lab::SensorArray, height, spacing, reach, far_offset)
ASTRAL_REFLECT(lab::LabStats, broken_joints, detached_joints, batch_ray_hits, motor_updates)
