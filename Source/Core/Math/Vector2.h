#pragma once

#include <cmath>
#include <cstdint>

namespace AstralEngine {

    /// <summary>
    /// 2D vector class for Astral Engine math operations
    /// Compatible with ImGui integration
    /// </summary>
    class Vector2 {
    public:
        float x, y;

        // Constructors
        constexpr Vector2() : x(0.0f), y(0.0f) {}
        constexpr Vector2(float _x, float _y) : x(_x), y(_y) {}
        constexpr Vector2(float scalar) : x(scalar), y(scalar) {}

        // Copy constructor
        constexpr Vector2(const Vector2& other) : x(other.x), y(other.y) {}

        // Assignment operator
        constexpr Vector2& operator=(const Vector2& other) {
            x = other.x;
            y = other.y;
            return *this;
        }

        // Arithmetic operators
        constexpr Vector2 operator+(const Vector2& other) const {
            return Vector2(x + other.x, y + other.y);
        }

        constexpr Vector2 operator-(const Vector2& other) const {
            return Vector2(x - other.x, y - other.y);
        }

        constexpr Vector2 operator*(float scalar) const {
            return Vector2(x * scalar, y * scalar);
        }

        constexpr Vector2 operator/(float scalar) const {
            return Vector2(x / scalar, y / scalar);
        }

        // Compound assignment operators
        constexpr Vector2& operator+=(const Vector2& other) {
            x += other.x;
            y += other.y;
            return *this;
        }

        constexpr Vector2& operator-=(const Vector2& other) {
            x -= other.x;
            y -= other.y;
            return *this;
        }

        constexpr Vector2& operator*=(float scalar) {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        constexpr Vector2& operator/=(float scalar) {
            x /= scalar;
            y /= scalar;
            return *this;
        }

        // Comparison operators
        constexpr bool operator==(const Vector2& other) const {
            return x == other.x && y == other.y;
        }

        constexpr bool operator!=(const Vector2& other) const {
            return !(*this == other);
        }

        // Utility functions
        float Length() const {
            return std::sqrt(x * x + y * y);
        }

        constexpr float LengthSquared() const {
            return x * x + y * y;
        }

        Vector2 Normalized() const {
            float len = Length();
            return len > 0.0f ? Vector2(x / len, y / len) : Vector2(0.0f, 0.0f);
        }

        constexpr float Dot(const Vector2& other) const {
            return x * other.x + y * other.y;
        }

        // Static constants
        static constexpr Vector2 Zero() { return Vector2(0.0f, 0.0f); }
        static constexpr Vector2 One() { return Vector2(1.0f, 1.0f); }
        static constexpr Vector2 UnitX() { return Vector2(1.0f, 0.0f); }
        static constexpr Vector2 UnitY() { return Vector2(0.0f, 1.0f); }
    };

    // Scalar multiplication (reverse order)
    inline constexpr Vector2 operator*(float scalar, const Vector2& vec) {
        return Vector2(scalar * vec.x, scalar * vec.y);
    }

} // namespace AstralEngine