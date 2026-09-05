#pragma once

#include "Astral/Scene/Scene.hpp"
#include <glm/glm.hpp>

namespace Astral {

struct SDFRaycastHit {
    bool hasHit = false;
    float distance = 0.0f;
    glm::vec3 hitPoint{0.0f};
    glm::vec3 hitNormal{0.0f, 1.0f, 0.0f};
    EntityHandle hitEntity = NullEntityHandle;
};

/**
 * @brief CPU Analitik SDF Mesafe ve Carpisma Sorgu Sistemi
 *
 * GPU raymarching shader'i (SDFCompute.glsl) ile matematiksel olarak 1:1 uyumlu analitik
 * mesafe fonksiyonlarini (Sphere, Box, Torus, Plane, Capsule, Cylinder) ve CSG islemlerini
 * (Union, Subtract, Intersect, SmoothUnion, SmoothSubtract) CPU uzerinde degerlendirir.
 * Agir harici fizik motorlarina gerek kalmadan hafif, deterministik ve kesin SDF carpismasi saglar.
 */
class SDFWorldQuery {
public:
    // --- Registry-based overloads ---
    [[nodiscard]] static float QueryDistance(const Registry& reg, const glm::vec3& point);
    [[nodiscard]] static float QueryDistance(const Registry& reg, const glm::vec3& point, EntityHandle& outClosestEntity);
    [[nodiscard]] static glm::vec3 QueryNormal(const Registry& reg, const glm::vec3& point, float eps = 0.002f);
    [[nodiscard]] static bool Raycast(const Registry& reg, const glm::vec3& origin, const glm::vec3& direction,
                                      float maxDistance, SDFRaycastHit& outHit, uint32_t maxSteps = 128);
    static bool ResolveSphereCollision(const Registry& reg, glm::vec3& position, float radius,
                                       float skin = 0.01f, uint32_t maxIterations = 4);

    // --- Scene-based overloads (convenience) ---
    [[nodiscard]] static float QueryDistance(const Scene& scene, const glm::vec3& point);
    [[nodiscard]] static float QueryDistance(const Scene& scene, const glm::vec3& point, EntityHandle& outClosestEntity);
    [[nodiscard]] static glm::vec3 QueryNormal(const Scene& scene, const glm::vec3& point, float eps = 0.002f);
    [[nodiscard]] static bool Raycast(const Scene& scene, const glm::vec3& origin, const glm::vec3& direction,
                                      float maxDistance, SDFRaycastHit& outHit, uint32_t maxSteps = 128);
    static bool ResolveSphereCollision(const Scene& scene, glm::vec3& position, float radius,
                                       float skin = 0.01f, uint32_t maxIterations = 4);
};

} // namespace Astral
