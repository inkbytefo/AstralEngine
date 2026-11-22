// Wind and water fields (ADR-0029), the shader twin of astral/physics/fields.hpp: same hash, same noise, same
// formulas, so the CPU simulation and the GPU (vegetation, particles, clouds, water) read one world.
//   #include <astral/fields.glsl>
// A parity test (tests/integration/fields_gpu_test.cpp) compares both implementations. Change them together.
#ifndef ASTRAL_FIELDS_GLSL
#define ASTRAL_FIELDS_GLSL

// Same layout as the C++ WindDesc fields (8 x 4 bytes, scalar block layout).
struct AstralWind {
    vec3 velocity;
    float gust_strength;
    float gust_frequency;
    float turbulence;
    float turbulence_scale;
    uint seed;
};

// A water volume without the density and flow (those are plain reads): center, half size, waves.
struct AstralWater {
    vec3 center;
    float wave_height;
    vec3 half_extents;
    float wave_length;
    float wave_speed;
};

uint astral_field_hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint astral_field_hash3(int x, int y, int z, uint seed) {
    uint h = seed;
    h = astral_field_hash(h ^ (uint(x) * 0x9E3779B1u));
    h = astral_field_hash(h ^ (uint(y) * 0x85EBCA77u));
    h = astral_field_hash(h ^ (uint(z) * 0xC2B2AE3Du));
    return h;
}

float astral_field_rand(uint h) { return float(h >> 8) * (1.0 / 16777216.0); }

float astral_field_smooth(float t) { return t * t * (3.0 - 2.0 * t); }

// Smooth value noise in [0, 1).
float astral_field_noise(vec3 p, uint seed) {
    vec3 f = floor(p);
    ivec3 i = ivec3(f);
    vec3 u = vec3(astral_field_smooth(p.x - f.x), astral_field_smooth(p.y - f.y), astral_field_smooth(p.z - f.z));
    float c000 = astral_field_rand(astral_field_hash3(i.x, i.y, i.z, seed));
    float c100 = astral_field_rand(astral_field_hash3(i.x + 1, i.y, i.z, seed));
    float c010 = astral_field_rand(astral_field_hash3(i.x, i.y + 1, i.z, seed));
    float c110 = astral_field_rand(astral_field_hash3(i.x + 1, i.y + 1, i.z, seed));
    float c001 = astral_field_rand(astral_field_hash3(i.x, i.y, i.z + 1, seed));
    float c101 = astral_field_rand(astral_field_hash3(i.x + 1, i.y, i.z + 1, seed));
    float c011 = astral_field_rand(astral_field_hash3(i.x, i.y + 1, i.z + 1, seed));
    float c111 = astral_field_rand(astral_field_hash3(i.x + 1, i.y + 1, i.z + 1, seed));
    float x00 = mix(c000, c100, u.x);
    float x10 = mix(c010, c110, u.x);
    float x01 = mix(c001, c101, u.x);
    float x11 = mix(c011, c111, u.x);
    return mix(mix(x00, x10, u.y), mix(x01, x11, u.y), u.z);
}

// Wind of one source (global wind or a volume's) at p, t seconds.
vec3 astral_wind_of(AstralWind w, vec3 p, float t) {
    vec3 v = w.velocity;
    float speed = length(v);
    if (w.gust_strength > 0.0 && speed > 1e-4) {
        vec3 d = v / speed;
        float g = astral_field_noise(vec3(t * w.gust_frequency, dot(p, d) * 0.05, 0.0), w.seed) * 2.0 - 1.0;
        v = v + d * (g * w.gust_strength);
    }
    if (w.turbulence > 0.0) {
        float inv = 1.0 / max(w.turbulence_scale, 0.01);
        vec3 q = vec3(p.x * inv + t * 0.37, p.y * inv + t * 0.21, p.z * inv + t * 0.29);
        v = v + vec3(astral_field_noise(q, w.seed + 1u) * 2.0 - 1.0, astral_field_noise(q, w.seed + 2u) * 2.0 - 1.0,
                     astral_field_noise(q, w.seed + 3u) * 2.0 - 1.0) * w.turbulence;
    }
    return v;
}

// 0 outside the box, 1 deeper than `falloff` inside it, eased in between.
float astral_volume_weight(vec3 p, vec3 center, vec3 half_extents, float falloff) {
    vec3 q = half_extents - abs(p - center);
    float depth = min(q.x, min(q.y, q.z));
    if (depth <= 0.0) return 0.0;
    if (falloff <= 0.0) return 1.0;
    return astral_field_smooth(min(depth / falloff, 1.0));
}

// World y of the water surface of a volume at x, z.
float astral_water_surface_height(AstralWater w, float x, float z, float t) {
    float base = w.center.y + w.half_extents.y;
    if (w.wave_height <= 0.0) return base;
    float k = 6.2831853 / max(w.wave_length, 0.1);
    float omega = k * w.wave_speed;
    float a = k * (x * 0.8 + z * 0.6) - omega * t;
    float b = 1.7 * k * (x * -0.5 + z * 0.8660254) - 1.7 * omega * t;
    return base + w.wave_height * (0.6 * sin(a) + 0.4 * sin(b));
}

#endif
