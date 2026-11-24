#ifndef ASTRAL_PARTICLE_STATE_GLSL
#define ASTRAL_PARTICLE_STATE_GLSL
#include <astral/scene.glsl>

struct ParticleState { vec4 position; vec4 velocity; };
layout(buffer_reference, scalar, buffer_reference_align = 16) buffer ParticleStates { ParticleState data[]; };

float random_unit(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return float(x & 0xffffffu) / 16777216.0;
}

vec3 particle_velocity(uint key) {
    return vec3(2.0 * random_unit(key) - 1.0, random_unit(key + 1u), 2.0 * random_unit(key + 2u) - 1.0) * 3.0;
}
#endif
