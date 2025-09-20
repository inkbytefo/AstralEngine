#pragma once

#include <cmath>
#include <cstdint>
#include "Vector2.h"

namespace AstralEngine {

    /// <summary>
    /// 4D vector class for Astral Engine math operations
    /// Compatible with ImGui integration
    /// </summary>
    class Vector4 {
    public:
        float x, y, z, w;

        // Constructors
        constexpr Vector4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
        constexpr Vector4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
        constexpr Vector4(float scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}
        constexpr Vector4(const Vector2& xy, float _z, float _w) : x(xy.x), y(xy.y), z(_z), w(_w) {}
        constexpr Vector4(float _x, float _y, const Vector2& zw) : x(_x), y(_y), z(zw.x), w(zw.y) {}

        // Copy constructor
        constexpr Vector4(const Vector4& other) : x(other.x), y(other.y), z(other.z), w(other.w) {}

        // Assignment operator
        constexpr Vector4& operator=(const Vector4& other) {
            x = other.x;
            y = other.y;
            z = other.z;
            w = other.w;
            return *this;
        }

        // Arithmetic operators
        constexpr Vector4 operator+(const Vector4& other) const {
            return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
        }

        constexpr Vector4 operator-(const Vector4& other) const {
            return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        constexpr Vector4 operator*(float scalar) const {
            return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
        }

        constexpr Vector4 operator/(float scalar) const {
            return Vector4(x / scalar, y / scalar, z / scalar, w / scalar);
        }

        // Compound assignment operators
        constexpr Vector4& operator+=(const Vector4& other) {
            x += other.x;
            y += other.y;
            z += other.z;
            w += other.w;
            return *this;
        }

        constexpr Vector4& operator-=(const Vector4& other) {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            w -= other.w;
            return *this;
        }

        constexpr Vector4& operator*=(float scalar) {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            w *= scalar;
            return *this;
        }

        constexpr Vector4& operator/=(float scalar) {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            w /= scalar;
            return *this;
        }

        // Comparison operators
        constexpr bool operator==(const Vector4& other) const {
            return x == other.x && y == other.y && z == other.z && w == other.w;
        }

        constexpr bool operator!=(const Vector4& other) const {
            return !(*this == other);
        }

        // Utility functions
        float Length() const {
            return std::sqrt(x * x + y * y + z * z + w * w);
        }

        constexpr float LengthSquared() const {
            return x * x + y * y + z * z + w * w;
        }

        Vector4 Normalized() const {
            float len = Length();
            return len > 0.0f ? Vector4(x / len, y / len, z / len, w / len) : Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        constexpr float Dot(const Vector4& other) const {
            return x * other.x + y * other.y + z * other.z + w * other.w;
        }

        // Swizzle accessors for compatibility
        constexpr Vector2 xy() const { return Vector2(x, y); }
        constexpr Vector2 xz() const { return Vector2(x, z); }
        constexpr Vector2 xw() const { return Vector2(x, w); }
        constexpr Vector2 yz() const { return Vector2(y, z); }
        constexpr Vector2 yw() const { return Vector2(y, w); }
        constexpr Vector2 zw() const { return Vector2(z, w); }

        // Static constants
        static constexpr Vector4 Zero() { return Vector4(0.0f, 0.0f, 0.0f, 0.0f); }
        static constexpr Vector4 One() { return Vector4(1.0f, 1.0f, 1.0f, 1.0f); }
        static constexpr Vector4 UnitX() { return Vector4(1.0f, 0.0f, 0.0f, 0.0f); }
        static constexpr Vector4 UnitY() { return Vector4(0.0f, 1.0f, 0.0f, 0.0f); }
        static constexpr Vector4 UnitZ() { return Vector4(0.0f, 0.0f, 1.0f, 0.0f); }
        static constexpr Vector4 UnitW() { return Vector4(0.0f, 0.0f, 0.0f, 1.0f); }
    };

    // Scalar multiplication (reverse order)
    inline constexpr Vector4 operator*(float scalar, const Vector4& vec) {
        return Vector4(scalar * vec.x, scalar * vec.y, scalar * vec.z, scalar * vec.w);
    }

} // namespace AstralEngine