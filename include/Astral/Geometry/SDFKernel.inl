#ifndef ASTRAL_GEOMETRY_SDFKERNEL_INL
#define ASTRAL_GEOMETRY_SDFKERNEL_INL

#ifdef __cplusplus
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

#define SDF_INLINE inline
#define SDF_FLOAT float
#define SDF_VEC2 glm::vec2
#define SDF_VEC3 glm::vec3
#define SDF_VEC4 glm::vec4
#define SDF_UINT uint32_t
#define SDF_INT int32_t
#define SDF_INOUT(T) T&

namespace Astral {
namespace Geometry {

inline float sdf_min(float a, float b) { return std::min(a, b); }
inline float sdf_max(float a, float b) { return std::max(a, b); }
inline float sdf_abs(float a) { return std::abs(a); }
inline float sdf_clamp(float v, float lo, float hi) { return std::clamp(v, lo, hi); }
inline float sdf_mix(float a, float b, float t) { return a * (1.0f - t) + b * t; }
inline float sdf_length(const glm::vec2& v) { return glm::length(v); }
inline float sdf_length(const glm::vec3& v) { return glm::length(v); }
inline float sdf_dot(const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); }
inline glm::vec2 sdf_abs(const glm::vec2& v) { return glm::abs(v); }
inline glm::vec3 sdf_abs(const glm::vec3& v) { return glm::abs(v); }
inline glm::vec2 sdf_max(const glm::vec2& a, const glm::vec2& b) { return glm::max(a, b); }
inline glm::vec3 sdf_max(const glm::vec3& a, const glm::vec3& b) { return glm::max(a, b); }
inline glm::vec2 sdf_min(const glm::vec2& a, const glm::vec2& b) { return glm::min(a, b); }
inline glm::vec3 sdf_min(const glm::vec3& a, const glm::vec3& b) { return glm::min(a, b); }
inline glm::vec3 sdf_mix(const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); }

#else // GLSL Environment

#define SDF_INLINE
#define SDF_FLOAT float
#define SDF_VEC2 vec2
#define SDF_VEC3 vec3
#define SDF_VEC4 vec4
#define SDF_UINT uint
#define SDF_INT int
#define SDF_INOUT(T) inout T

#define sdf_min min
#define sdf_max max
#define sdf_abs abs
#define sdf_clamp clamp
#define sdf_mix mix
#define sdf_length length
#define sdf_dot dot

#endif

// ============================================================================
// Shared Primitive Distance Equations (Lipschitz-compliant, ||grad|| <= 1)
// ============================================================================

SDF_INLINE SDF_FLOAT sdSphere(SDF_VEC3 p, SDF_FLOAT r) {
    return sdf_length(p) - r;
}

SDF_INLINE SDF_FLOAT sdBox(SDF_VEC3 p, SDF_VEC3 b) {
    SDF_VEC3 q = sdf_abs(p) - b;
    return sdf_length(sdf_max(q, SDF_VEC3(0.0))) + sdf_min(sdf_max(q.x, sdf_max(q.y, q.z)), 0.0);
}

SDF_INLINE SDF_FLOAT sdTorus(SDF_VEC3 p, SDF_VEC2 t) {
    SDF_VEC2 q = SDF_VEC2(sdf_length(SDF_VEC2(p.x, p.z)) - t.x, p.y);
    return sdf_length(q) - t.y;
}

SDF_INLINE SDF_FLOAT sdPlane(SDF_VEC3 p, SDF_FLOAT h) {
    return p.y + h;
}

SDF_INLINE SDF_FLOAT sdCapsule(SDF_VEC3 p, SDF_FLOAT r, SDF_FLOAT h) {
    p.y -= sdf_clamp(p.y, 0.0, h);
    return sdf_length(p) - r;
}

SDF_INLINE SDF_FLOAT sdCylinder(SDF_VEC3 p, SDF_FLOAT r, SDF_FLOAT h) {
    SDF_VEC2 d = sdf_abs(SDF_VEC2(sdf_length(SDF_VEC2(p.x, p.z)), p.y)) - SDF_VEC2(r, h);
    return sdf_min(sdf_max(d.x, d.y), 0.0) + sdf_length(sdf_max(d, SDF_VEC2(0.0)));
}

// Canonical primitive evaluator in local object space
SDF_INLINE SDF_FLOAT evalPrimitiveDistance(SDF_VEC3 localP, SDF_UINT primitiveType, SDF_VEC4 dimensions) {
    if (primitiveType == 0u) {
        return sdSphere(localP, dimensions.x);
    } else if (primitiveType == 1u) {
        return sdBox(localP, SDF_VEC3(dimensions.x, dimensions.y, dimensions.z));
    } else if (primitiveType == 2u) {
        return sdTorus(localP, SDF_VEC2(dimensions.x, dimensions.y));
    } else if (primitiveType == 3u) {
        return sdPlane(localP, dimensions.x);
    } else if (primitiveType == 4u) {
        return sdCapsule(localP, dimensions.x, dimensions.y);
    } else if (primitiveType == 5u) {
        return sdCylinder(localP, dimensions.x, dimensions.y);
    }
    return sdSphere(localP, dimensions.x);
}

// ============================================================================
// Evaluation Result & CSG Operators with Surface Attribution
// ============================================================================

struct SDFHitResult {
    SDF_FLOAT distance;
    SDF_VEC3 albedo;
    SDF_FLOAT roughness;
    SDF_FLOAT metallic;
    SDF_UINT surfaceId;
    SDF_INT hitIndex;
    SDF_FLOAT confidence; // 1.0 = sharp single surface, <1.0 = blend transition zone
};

SDF_INLINE SDF_FLOAT combineDistance(
    SDF_FLOAT accumDist,
    bool isFirst,
    SDF_FLOAT primDist,
    SDF_UINT operation,
    SDF_FLOAT blendFactor
) {
    if (isFirst) return primDist;

    if (operation == 0u) { // Union
        return sdf_min(accumDist, primDist);
    } else if (operation == 1u) { // Subtract
        return sdf_max(accumDist, -primDist);
    } else if (operation == 2u) { // Intersect
        return sdf_max(accumDist, primDist);
    } else if (operation == 3u) { // Smooth Union
        SDF_FLOAT k = sdf_max(blendFactor, 0.001);
        SDF_FLOAT h = sdf_clamp(0.5 + 0.5 * (accumDist - primDist) / k, 0.0, 1.0);
        return sdf_mix(accumDist, primDist, h) - k * h * (1.0 - h);
    } else if (operation == 4u) { // Smooth Subtract
        SDF_FLOAT k = sdf_max(blendFactor, 0.001);
        SDF_FLOAT h = sdf_clamp(0.5 - 0.5 * (accumDist + primDist) / k, 0.0, 1.0);
        return sdf_mix(accumDist, -primDist, h) + k * h * (1.0 - h);
    }
    return accumDist;
}

SDF_INLINE void combinePrimitive(
    SDF_INOUT(SDFHitResult) accum,
    bool isFirst,
    SDF_FLOAT primDist,
    SDF_VEC3 primAlbedo,
    SDF_FLOAT primRoughness,
    SDF_FLOAT primMetallic,
    SDF_UINT primSurfaceId,
    SDF_INT primIndex,
    SDF_UINT operation,
    SDF_FLOAT blendFactor
) {
    if (isFirst) {
        accum.distance = primDist;
        accum.albedo = primAlbedo;
        accum.roughness = primRoughness;
        accum.metallic = primMetallic;
        accum.surfaceId = primSurfaceId;
        accum.hitIndex = primIndex;
        accum.confidence = 1.0;
        return;
    }

    if (operation == 0u) { // Union
        if (primDist < accum.distance) {
            accum.distance = primDist;
            accum.albedo = primAlbedo;
            accum.roughness = primRoughness;
            accum.metallic = primMetallic;
            accum.surfaceId = primSurfaceId;
            accum.hitIndex = primIndex;
            accum.confidence = 1.0;
        }
    } else if (operation == 1u) { // Subtract
        accum.distance = sdf_max(accum.distance, -primDist);
        if (-primDist > accum.distance - 0.001) {
            // Cut surface takes carving material
            accum.surfaceId = primSurfaceId;
            accum.hitIndex = primIndex;
            accum.albedo = primAlbedo;
            accum.roughness = primRoughness;
            accum.metallic = primMetallic;
        }
    } else if (operation == 2u) { // Intersect
        if (primDist > accum.distance) {
            accum.distance = primDist;
            accum.albedo = primAlbedo;
            accum.roughness = primRoughness;
            accum.metallic = primMetallic;
            accum.surfaceId = primSurfaceId;
            accum.hitIndex = primIndex;
            accum.confidence = 1.0;
        }
    } else if (operation == 3u) { // Smooth Union
        SDF_FLOAT k = sdf_max(blendFactor, 0.001);
        SDF_FLOAT h = sdf_clamp(0.5 + 0.5 * (accum.distance - primDist) / k, 0.0, 1.0);
        accum.distance = sdf_mix(accum.distance, primDist, h) - k * h * (1.0 - h);
        if (h > 0.5) {
            accum.surfaceId = primSurfaceId;
            accum.hitIndex = primIndex;
        }
        accum.albedo = sdf_mix(accum.albedo, primAlbedo, h);
        accum.roughness = sdf_mix(accum.roughness, primRoughness, h);
        accum.metallic = sdf_mix(accum.metallic, primMetallic, h);
        SDF_FLOAT blendConfidence = 1.0 - 4.0 * h * (1.0 - h);
        accum.confidence = sdf_min(accum.confidence, blendConfidence);
    } else if (operation == 4u) { // Smooth Subtract
        SDF_FLOAT k = sdf_max(blendFactor, 0.001);
        SDF_FLOAT h = sdf_clamp(0.5 - 0.5 * (accum.distance + primDist) / k, 0.0, 1.0);
        accum.distance = sdf_mix(accum.distance, -primDist, h) + k * h * (1.0 - h);
        if (h > 0.5) {
            accum.surfaceId = primSurfaceId;
            accum.hitIndex = primIndex;
        }
        accum.albedo = sdf_mix(accum.albedo, primAlbedo, h);
        accum.roughness = sdf_mix(accum.roughness, primRoughness, h);
        accum.metallic = sdf_mix(accum.metallic, primMetallic, h);
        SDF_FLOAT blendConfidence = 1.0 - 4.0 * h * (1.0 - h);
        accum.confidence = sdf_min(accum.confidence, blendConfidence);
    }
}

SDF_INLINE void combinePrimitive(
    SDF_INOUT(SDFHitResult) accum,
    bool isFirst,
    SDF_FLOAT primDist,
    SDF_VEC3 primAlbedo,
    SDF_FLOAT primRoughness,
    SDF_FLOAT primMetallic,
    SDF_UINT primSurfaceId,
    SDF_UINT operation,
    SDF_FLOAT blendFactor
) {
    combinePrimitive(accum, isFirst, primDist, primAlbedo, primRoughness, primMetallic, primSurfaceId, SDF_INT(-1), operation, blendFactor);
}

#ifdef __cplusplus
} // namespace Geometry
} // namespace Astral
#endif

#endif // ASTRAL_GEOMETRY_SDFKERNEL_INL
