#pragma once

// CPU implementation of the SDF model. Mirrors shaders/include/Sdf.glsl exactly;
// tests compare both. Used for physics collision, gameplay queries and picking.

#include "Astral/Render/RenderTypes.hpp"

#include <algorithm>
#include <cmath>
#include <span>

namespace Astral::Render::Sdf {

inline float Sphere(const glm::vec3& p, float r) { return glm::length(p) - r; }

inline float Box(const glm::vec3& p, const glm::vec3& halfExtents, float rounding) {
    glm::vec3 q = glm::abs(p) - (halfExtents - glm::vec3(rounding));
    return glm::length(glm::max(q, glm::vec3(0.0f))) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f) - rounding;
}

inline float Torus(const glm::vec3& p, float ringRadius, float tubeRadius) {
    glm::vec2 q(glm::length(glm::vec2(p.x, p.z)) - ringRadius, p.y);
    return glm::length(q) - tubeRadius;
}

inline float Capsule(glm::vec3 p, float radius, float halfLength) {
    p.y -= std::clamp(p.y, -halfLength, halfLength);
    return glm::length(p) - radius;
}

inline float Cylinder(const glm::vec3& p, float radius, float halfHeight, float rounding) {
    glm::vec2 d = glm::abs(glm::vec2(glm::length(glm::vec2(p.x, p.z)), p.y)) - glm::vec2(radius - rounding, halfHeight - rounding);
    return std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, glm::vec2(0.0f))) - rounding;
}

inline float Primitive(const SdfShapeDesc& s, const glm::vec3& worldPoint) {
    glm::vec3 p = glm::conjugate(s.rotation) * (worldPoint - s.position);
    switch (s.primitive) {
        case SdfPrimitive::Sphere:   return Sphere(p, s.size.x);
        case SdfPrimitive::Box:      return Box(p, glm::vec3(s.size), std::min(s.size.w, std::min(s.size.x, std::min(s.size.y, s.size.z))));
        case SdfPrimitive::Torus:    return Torus(p, s.size.x, s.size.y);
        case SdfPrimitive::Capsule:  return Capsule(p, s.size.x, s.size.y);
        case SdfPrimitive::Cylinder: return Cylinder(p, s.size.x, s.size.y, std::min(s.size.w, std::min(s.size.x, s.size.y)));
    }
    return Sphere(p, s.size.x);
}

// Radius of a sphere around position that contains the primitive (rotation independent).
inline float BoundRadius(const SdfShapeDesc& s) {
    switch (s.primitive) {
        case SdfPrimitive::Sphere:   return s.size.x;
        case SdfPrimitive::Box:      return glm::length(glm::vec3(s.size));
        case SdfPrimitive::Torus:    return s.size.x + s.size.y;
        case SdfPrimitive::Capsule:  return s.size.x + s.size.y;
        case SdfPrimitive::Cylinder: return glm::length(glm::vec2(s.size.x, s.size.y));
    }
    return glm::length(glm::vec3(s.size));
}

inline float SmoothUnion(float a, float b, float k) {
    float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return glm::mix(b, a, h) - k * h * (1.0f - h);
}

inline float SmoothSubtract(float a, float b, float k) { // removes b from a
    float h = std::clamp(0.5f - 0.5f * (a + b) / k, 0.0f, 1.0f);
    return glm::mix(a, -b, h) + k * h * (1.0f - h);
}

inline float Combine(float accumulated, float d, SdfOperation op, float blend) {
    float k = std::max(blend, 0.001f);
    switch (op) {
        case SdfOperation::Union:          return std::min(accumulated, d);
        case SdfOperation::Subtract:       return std::max(accumulated, -d);
        case SdfOperation::Intersect:      return std::max(accumulated, d);
        case SdfOperation::SmoothUnion:    return SmoothUnion(accumulated, d, k);
        case SdfOperation::SmoothSubtract: return SmoothSubtract(accumulated, d, k);
    }
    return accumulated;
}

inline constexpr float kFar = 1e10f;

// Distance to the shape list. Shapes must already be in evaluation order.
inline float Evaluate(std::span<const SdfShapeDesc> shapes, const glm::vec3& p) {
    float d = kFar;
    for (const SdfShapeDesc& s : shapes) d = Combine(d, Primitive(s, p), s.operation, s.blend);
    return d;
}

// Index of the shape that defines the surface at p (for picking/materials), or -1.
inline int DominantShape(std::span<const SdfShapeDesc> shapes, const glm::vec3& p) {
    float d = kFar;
    int best = -1;
    for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
        const SdfShapeDesc& s = shapes[i];
        float di = Primitive(s, p);
        float next = Combine(d, di, s.operation, s.blend);
        bool additive = s.operation == SdfOperation::Union || s.operation == SdfOperation::SmoothUnion;
        if (additive && di <= d) best = i;
        if (s.operation == SdfOperation::Intersect && di > d) best = i;
        d = next;
    }
    return best;
}

inline glm::vec3 Normal(std::span<const SdfShapeDesc> shapes, const glm::vec3& p, float h = 1e-3f) {
    const glm::vec2 k(1.0f, -1.0f);
    glm::vec3 n = glm::vec3(k.x, k.y, k.y) * Evaluate(shapes, p + glm::vec3(k.x, k.y, k.y) * h) +
                  glm::vec3(k.y, k.y, k.x) * Evaluate(shapes, p + glm::vec3(k.y, k.y, k.x) * h) +
                  glm::vec3(k.y, k.x, k.y) * Evaluate(shapes, p + glm::vec3(k.y, k.x, k.y) * h) +
                  glm::vec3(k.x, k.x, k.x) * Evaluate(shapes, p + glm::vec3(k.x, k.x, k.x) * h);
    float len = glm::length(n);
    return len > 0.0f ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
}

} // namespace Astral::Render::Sdf
