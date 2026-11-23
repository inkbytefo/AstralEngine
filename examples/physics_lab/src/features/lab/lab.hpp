#pragma once

#include <astral/app/feature.hpp>

namespace lab {

struct MotorOscillator {
    float frequency = 0.6f;
    float amplitude = 1.0f;
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
ASTRAL_REFLECT(lab::LabStats, broken_joints, detached_joints, batch_ray_hits, motor_updates)
