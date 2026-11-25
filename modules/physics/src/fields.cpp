#include <astral/physics/fields.hpp>

#include <algorithm>
#include <cmath>

namespace astral {

std::uint32_t field_hash(std::uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

namespace {

std::uint32_t hash3(std::int32_t x, std::int32_t y, std::int32_t z, std::uint32_t seed) noexcept {
    std::uint32_t h = seed;
    h = field_hash(h ^ (static_cast<std::uint32_t>(x) * 0x9E3779B1u));
    h = field_hash(h ^ (static_cast<std::uint32_t>(y) * 0x85EBCA77u));
    h = field_hash(h ^ (static_cast<std::uint32_t>(z) * 0xC2B2AE3Du));
    return h;
}

float rand01(std::uint32_t h) noexcept { return static_cast<float>(h >> 8) * (1.0f / 16777216.0f); }
float mix(float a, float b, float t) noexcept { return a + (b - a) * t; }
float smooth(float t) noexcept { return t * t * (3.0f - 2.0f * t); }

} // namespace

/// Lattice coordinate of floor(v): wrapped into int32 with unsigned arithmetic (no overflow UB, no
/// out-of-range float-to-int cast); non-finite input maps to cell 0.
static std::int32_t lattice(float v) noexcept {
    if (!std::isfinite(v)) return 0;
    const double wrapped = std::fmod(static_cast<double>(v), 4294967296.0); // exact for any float
    const auto bits = static_cast<std::uint32_t>(static_cast<std::int64_t>(wrapped));
    return static_cast<std::int32_t>(bits);
}

static std::int32_t lattice_step(std::int32_t i, int d) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(i) + static_cast<std::uint32_t>(d));
}

float field_noise(Vec3 p, std::uint32_t seed) noexcept {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return 0.5f; // neutral sample
    const float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
    const std::int32_t ix = lattice(fx), iy = lattice(fy), iz = lattice(fz);
    const float ux = smooth(p.x - fx), uy = smooth(p.y - fy), uz = smooth(p.z - fz);
    const auto corner = [&](int dx, int dy, int dz) { return rand01(hash3(lattice_step(ix, dx), lattice_step(iy, dy), lattice_step(iz, dz), seed)); };
    const float x00 = mix(corner(0, 0, 0), corner(1, 0, 0), ux);
    const float x10 = mix(corner(0, 1, 0), corner(1, 1, 0), ux);
    const float x01 = mix(corner(0, 0, 1), corner(1, 0, 1), ux);
    const float x11 = mix(corner(0, 1, 1), corner(1, 1, 1), ux);
    return mix(mix(x00, x10, uy), mix(x01, x11, uy), uz);
}

Vec3 wind_of(const WindDesc& w, Vec3 p, float t) noexcept {
    Vec3 v = w.velocity;
    const float speed = length(v);
    if (w.gust_strength > 0.0f && speed > 1e-4f) {
        const Vec3 d = v / speed;
        const float g = field_noise({t * w.gust_frequency, dot(p, d) * 0.05f, 0.0f}, w.seed) * 2.0f - 1.0f;
        v = v + d * (g * w.gust_strength);
    }
    if (w.turbulence > 0.0f) {
        const float inv = 1.0f / std::max(w.turbulence_scale, 0.01f);
        const Vec3 q{p.x * inv + t * 0.37f, p.y * inv + t * 0.21f, p.z * inv + t * 0.29f};
        v = v + Vec3{field_noise(q, w.seed + 1u) * 2.0f - 1.0f, field_noise(q, w.seed + 2u) * 2.0f - 1.0f,
                     field_noise(q, w.seed + 3u) * 2.0f - 1.0f} * w.turbulence;
    }
    return v;
}

float volume_weight(Vec3 p, Vec3 c, Vec3 h, float falloff) noexcept {
    const float depth = std::min({h.x - std::abs(p.x - c.x), h.y - std::abs(p.y - c.y), h.z - std::abs(p.z - c.z)});
    if (depth <= 0.0f) return 0.0f;
    if (falloff <= 0.0f) return 1.0f;
    return smooth(std::min(depth / falloff, 1.0f));
}

float water_surface_height(const WaterVolume& w, float x, float z, float t) noexcept {
    const float base = w.center.y + w.half_extents.y;
    if (w.wave_height <= 0.0f) return base;
    const float k = 6.2831853f / std::max(w.wave_length, 0.1f);
    const float omega = k * w.wave_speed;
    const float a = k * (x * 0.8f + z * 0.6f) - omega * t;
    const float b = 1.7f * k * (x * -0.5f + z * 0.8660254f) - 1.7f * omega * t;
    return base + w.wave_height * (0.6f * std::sin(a) + 0.4f * std::sin(b));
}

Vec3 FieldWorld::wind_at(Vec3 position, double time) const noexcept {
    const auto t = static_cast<float>(time);
    Vec3 wind = wind_of(global_, position, t);
    for (const WindVolume& v : winds_) {
        const float weight = volume_weight(position, v.center, v.half_extents, v.falloff);
        if (weight <= 0.0f) continue;
        const Vec3 local = wind_of(v.wind, position, t);
        wind = v.replace ? lerp(wind, local, weight) : wind + local * weight;
    }
    return wind;
}

WaterSample FieldWorld::water_at(Vec3 position, double time) const noexcept {
    const auto t = static_cast<float>(time);
    WaterSample best;
    for (const WaterVolume& w : waters_) {
        if (std::abs(position.x - w.center.x) > w.half_extents.x || std::abs(position.z - w.center.z) > w.half_extents.z) continue;
        if (position.y < w.center.y - w.half_extents.y) continue; // below the floor of the volume
        const float surface = water_surface_height(w, position.x, position.z, t);
        const float depth = surface - position.y;
        if (depth <= 0.0f || (best.inside && depth <= best.depth)) continue;
        best = {true, surface, depth, w.flow, w.density};
    }
    return best;
}

} // namespace astral
