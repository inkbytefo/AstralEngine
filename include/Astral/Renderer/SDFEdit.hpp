#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include "Astral/Geometry/SDFSceneSnapshot.hpp"

namespace Astral {

enum class PrimitiveType : uint32_t {
    Sphere = 0,
    Box = 1,
    Torus = 2,
    Plane = 3,
    Capsule = 4,
    Cylinder = 5
};

enum class CSGOperation : uint32_t {
    Union = 0,
    Subtract = 1,
    Intersect = 2,
    SmoothUnion = 3,
    SmoothSubtract = 4
};

/// Geriye donuk test uyumlulugu icin eski ara yapı (SDFEditGPU).
/// Calisma zamaninda dogrudan SDFPrimitiveRecord kullanilir.
struct alignas(16) LegacySDFEdit {
    glm::vec3 position{0.0f};
    float pad1 = 0.0f;

    glm::vec4 rotation{0.0f, 0.0f, 0.0f, 1.0f}; // quaternion

    glm::vec3 scale{1.0f};
    uint32_t primitiveType = 0;

    uint32_t operation = 3; // SmoothUnion varsayilan
    float blendFactor = 0.25f;
    uint32_t isDynamic = 0;
    float pad2 = 0.0f;

    glm::vec3 albedo{1.0f};
    float roughness = 0.5f;

    float metallic = 0.0f;
    float prevPosX = 0.0f;
    float prevPosY = 0.0f;
    float prevPosZ = 0.0f;

    glm::vec4 prevRotation{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 prevScale{1.0f};

    void SetPrevPosition(const glm::vec3& p) {
        prevPosX = p.x;
        prevPosY = p.y;
        prevPosZ = p.z;
    }

    glm::vec3 GetPrevPosition() const {
        return glm::vec3(prevPosX, prevPosY, prevPosZ);
    }

    [[nodiscard]] SDFPrimitiveRecord ToPrimitiveRecord(uint32_t order = 0, uint32_t surfId = 0) const {
        SDFPrimitiveRecord rec{};
        glm::mat4 m = glm::translate(glm::mat4(1.0f), position) *
                      glm::mat4_cast(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
        rec.invTransform = glm::inverse(m);
        rec.dimensions = glm::vec4(scale, 0.0f);
        rec.albedoRoughness = glm::vec4(albedo, roughness);
        rec.metallicParams = glm::vec4(metallic, blendFactor, 1.0f, static_cast<float>(isDynamic));
        rec.primitiveType = primitiveType;
        rec.operation = operation;
        rec.csgOrder = order;
        rec.surfaceId = surfId;
        return rec;
    }
};

static_assert(sizeof(LegacySDFEdit) == 128, "LegacySDFEdit struct boyutu tam olarak 128 bayt olmalidir!");

/// Raymarching analitik primitif degerlendirme butcesi ve SSBO sabit tahsis limiti.
/// Vulkan 1.4 SSBO donanim kisitlamasi degil, 60+ FPS hedeflenen isin adimi basina (128 max adim)
/// O(N) analitik test maliyetini ve 32 KB (256 * 128 B) onbellek dostu tampon boyutunu optimize eden
/// merkezi sahne limiti.
inline constexpr size_t MAX_SDF_EDITS = 256;

} // namespace Astral
