#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "Astral/Geometry/SDFKernel.inl"
#include "Astral/Core/Registry.hpp"

namespace Astral {

struct SDFSurfaceKey {
    uint32_t entityIndex = 0;
    uint32_t entityGeneration = 0;
    uint32_t sceneInstanceId = 0;
    uint32_t subPrimitiveIndex = 0;

    [[nodiscard]] uint32_t Hash() const noexcept {
        uint32_t h = 2166136261u;
        auto hashWord = [&h](uint32_t w) {
            h ^= (w & 0xFF);        h *= 16777619u;
            h ^= ((w >> 8) & 0xFF);  h *= 16777619u;
            h ^= ((w >> 16) & 0xFF); h *= 16777619u;
            h ^= ((w >> 24) & 0xFF); h *= 16777619u;
        };
        hashWord(entityIndex);
        hashWord(entityGeneration);
        hashWord(sceneInstanceId);
        hashWord(subPrimitiveIndex);
        return (h == 0u) ? 1u : h; // 0 reserved for miss/background
    }

    bool operator==(const SDFSurfaceKey& o) const noexcept {
        return entityIndex == o.entityIndex &&
               entityGeneration == o.entityGeneration &&
               sceneInstanceId == o.sceneInstanceId &&
               subPrimitiveIndex == o.subPrimitiveIndex;
    }
};

/// 128 bytes, 16-byte aligned for GPU SSBO storage
struct alignas(16) SDFPrimitiveRecord {
    glm::mat4 invTransform{1.0f};          // 64 bytes - transforms world point to local primitive space
    glm::vec4 dimensions{1.0f};            // 16 bytes - shape parameters
    glm::vec4 albedoRoughness{1.0f, 1.0f, 1.0f, 0.5f}; // 16 bytes - rgb: albedo, w: roughness
    glm::vec4 metallicParams{0.0f, 0.25f, 1.0f, 0.0f}; // 16 bytes - x: metallic, y: blendFactor, z: conservativeScale, w: isDynamic
    uint32_t primitiveType = 0;            // 4 bytes
    uint32_t operation = 0;                // 4 bytes
    uint32_t csgOrder = 0;                 // 4 bytes
    uint32_t surfaceId = 0;                // 4 bytes
};
static_assert(sizeof(SDFPrimitiveRecord) == 128, "SDFPrimitiveRecord must be exactly 128 bytes!");

class SDFSceneSnapshot {
public:
    SDFSceneSnapshot() = default;

    /// Extracts snapshot from registry, sorting deterministically by (csgOrder, surfaceId)
    /// and identically excluding singular/non-invertible transforms.
    [[nodiscard]] static SDFSceneSnapshot Extract(const Registry& registry, uint32_t sceneInstanceId = 1);

    [[nodiscard]] const std::vector<SDFPrimitiveRecord>& GetRecords() const noexcept { return m_Records; }
    [[nodiscard]] const std::vector<EntityHandle>& GetEntities() const noexcept { return m_Entities; }
    [[nodiscard]] uint32_t GetRecordCount() const noexcept { return static_cast<uint32_t>(m_Records.size()); }
    [[nodiscard]] uint64_t GetRevision() const noexcept { return m_Revision; }

    /// Evaluates full SDF hit result at world coordinate using unified SDFKernel
    [[nodiscard]] Geometry::SDFHitResult Evaluate(const glm::vec3& worldPos) const;

    /// Evaluates distance only at world coordinate
    [[nodiscard]] float EvaluateDistance(const glm::vec3& worldPos) const;

private:
    std::vector<SDFPrimitiveRecord> m_Records;
    std::vector<EntityHandle> m_Entities;
    uint64_t m_Revision = 0;
};

} // namespace Astral
