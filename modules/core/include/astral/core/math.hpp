#pragma once

// Small, dependency-free 3D math. Conventions:
//   - right-handed world, +Y up, camera looks down -Z
//   - Mat4 is column-major (m[column][row]), vectors are columns: v' = M * v
//   - clip space is Vulkan's: Y down, depth in [0, 1]

#include <cmath>
#include <numbers>

namespace astral {

constexpr float radians(float degrees) noexcept { return degrees * (std::numbers::pi_v<float> / 180.0f); }
constexpr float degrees(float radians) noexcept { return radians * (180.0f / std::numbers::pi_v<float>); }

struct Vec2 {
    float x = 0, y = 0;
    constexpr bool operator==(const Vec2&) const = default;
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    constexpr bool operator==(const Vec3&) const = default;
};

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    constexpr bool operator==(const Vec4&) const = default;
    constexpr float& operator[](int i) noexcept { return (&x)[i]; }
    constexpr float operator[](int i) const noexcept { return (&x)[i]; }
};

// --- Vec2 / Vec3 / Vec4 operators ---------------------------------------------------

constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
constexpr Vec2 operator*(Vec2 a, float s) noexcept { return {a.x * s, a.y * s}; }

constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator-(Vec3 a) noexcept { return {-a.x, -a.y, -a.z}; }
constexpr Vec3 operator*(Vec3 a, float s) noexcept { return {a.x * s, a.y * s, a.z * s}; }
constexpr Vec3 operator*(float s, Vec3 a) noexcept { return a * s; }
constexpr Vec3 operator*(Vec3 a, Vec3 b) noexcept { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
constexpr Vec3 operator/(Vec3 a, float s) noexcept { return {a.x / s, a.y / s, a.z / s}; }
constexpr Vec3& operator+=(Vec3& a, Vec3 b) noexcept { return a = a + b; }
constexpr Vec3& operator-=(Vec3& a, Vec3 b) noexcept { return a = a - b; }
constexpr Vec3& operator*=(Vec3& a, float s) noexcept { return a = a * s; }

constexpr Vec4 operator+(Vec4 a, Vec4 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
constexpr Vec4 operator*(Vec4 a, float s) noexcept { return {a.x * s, a.y * s, a.z * s, a.w * s}; }

constexpr float dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr float dot(Vec4 a, Vec4 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 v) noexcept { return std::sqrt(dot(v, v)); }
/// Returns the zero vector for a zero-length input instead of NaN.
inline Vec3 normalize(Vec3 v) noexcept {
    const float len = length(v);
    return len > 0.0f ? v / len : Vec3{};
}
constexpr Vec3 lerp(Vec3 a, Vec3 b, float t) noexcept { return a + (b - a) * t; }

// --- Quat --------------------------------------------------------------------------

/// Unit quaternion rotation (x, y, z = vector part, w = scalar part).
struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
    constexpr bool operator==(const Quat&) const = default;

    static constexpr Quat identity() noexcept { return {}; }
    static Quat from_axis_angle(Vec3 axis, float angle) noexcept {
        const Vec3 n = normalize(axis);
        const float s = std::sin(angle * 0.5f);
        return {n.x * s, n.y * s, n.z * s, std::cos(angle * 0.5f)};
    }
};

/// Hamilton product: (a * b) applies b first, then a.
constexpr Quat operator*(Quat a, Quat b) noexcept {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

constexpr Vec3 operator*(Quat q, Vec3 v) noexcept {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

constexpr Quat conjugate(Quat q) noexcept { return {-q.x, -q.y, -q.z, q.w}; }

inline Quat normalize(Quat q) noexcept {
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return len > 0.0f ? Quat{q.x / len, q.y / len, q.z / len, q.w / len} : Quat{};
}

// --- Mat4 --------------------------------------------------------------------------

struct Mat4 {
    Vec4 cols[4]{};

    static constexpr Mat4 identity() noexcept {
        return {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    }
    constexpr Vec4& operator[](int c) noexcept { return cols[c]; }
    constexpr const Vec4& operator[](int c) const noexcept { return cols[c]; }
    constexpr bool operator==(const Mat4&) const = default;
};

constexpr Vec4 operator*(const Mat4& m, Vec4 v) noexcept {
    return m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3] * v.w;
}

constexpr Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
    return {{a * b[0], a * b[1], a * b[2], a * b[3]}};
}

constexpr Mat4 translate(Vec3 t) noexcept {
    Mat4 m = Mat4::identity();
    m[3] = {t.x, t.y, t.z, 1};
    return m;
}

constexpr Mat4 scale(Vec3 s) noexcept {
    return {{{s.x, 0, 0, 0}, {0, s.y, 0, 0}, {0, 0, s.z, 0}, {0, 0, 0, 1}}};
}

constexpr Mat4 rotate(Quat q) noexcept {
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return {{{1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0},
             {2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0},
             {2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0},
             {0, 0, 0, 1}}};
}

/// translate * rotate * scale — the usual object-to-world transform.
constexpr Mat4 compose(Vec3 position, Quat rotation, Vec3 scaling) noexcept {
    return translate(position) * rotate(rotation) * scale(scaling);
}

constexpr Mat4 transpose(const Mat4& m) noexcept {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r[c][row] = m[row][c];
        }
    }
    return r;
}

/// General 4x4 inverse (cofactor expansion). Returns identity for a singular matrix.
Mat4 inverse(const Mat4& m) noexcept;

/// View matrix for a camera at `eye` looking at `target`.
Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept;

/// Perspective projection into Vulkan clip space (Y down, depth 0 at near, 1 at far).
Mat4 perspective(float fov_y, float aspect, float near_plane, float far_plane) noexcept;

/// Orthographic projection into Vulkan clip space.
Mat4 orthographic(float left, float right, float bottom, float top, float near_plane,
                  float far_plane) noexcept;

} // namespace astral
