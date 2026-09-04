#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/TransformSystem.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <cstring>
#include <algorithm>

namespace Astral {

void ExtractRenderData(Registry& registry, std::vector<SDFEditGPU>& outEdits, std::vector<EntityHandle>& outEntities) {
    outEdits.clear();
    outEntities.clear();
    outEdits.reserve(256);
    outEntities.reserve(256);

    auto& transforms = registry.GetView<TransformComponent>();

    for (auto&& [entity, transform] : transforms) {
        (void)transform;
        if (registry.HasComponent<SDFComponent>(entity)) {
            const auto& sdf = registry.GetComponent<SDFComponent>(entity);

            const glm::mat4 worldMatrix = GetWorldTransformMatrix(registry, entity);
            glm::vec3 worldPosition;
            glm::quat worldRotation;
            glm::vec3 worldScale;
            DecomposeTransformMatrix(worldMatrix, worldPosition, worldRotation, worldScale);

            SDFEditGPU gpuData{};
            gpuData.position = worldPosition;
            gpuData.rotation = glm::vec4(
                worldRotation.x,
                worldRotation.y,
                worldRotation.z,
                worldRotation.w
            );
            gpuData.scale = worldScale;
            gpuData.primitiveType = sdf.primitiveType;
            gpuData.operation = sdf.operation;
            gpuData.blendFactor = sdf.blendFactor;
            gpuData.isDynamic = sdf.isDynamic;
            gpuData.albedo = sdf.albedo;
            gpuData.roughness = sdf.roughness;
            gpuData.metallic = sdf.metallic;

            outEdits.push_back(gpuData);
            outEntities.push_back(entity);
        }
    }
}

void ExtractRenderData(Registry& registry, std::vector<SDFEditGPU>& outEdits) {
    std::vector<EntityHandle> unusedEntities;
    ExtractRenderData(registry, outEdits, unusedEntities);
}

void ExtractAndUploadRenderData(Registry& registry, void* mappedGpuBuffer, uint32_t& outEditCount) {
    if (!mappedGpuBuffer) {
        outEditCount = 0;
        return;
    }

    std::vector<SDFEditGPU> uploadBuffer;
    ExtractRenderData(registry, uploadBuffer);

    outEditCount = static_cast<uint32_t>(uploadBuffer.size());
    if (outEditCount > 0) {
        std::memcpy(mappedGpuBuffer, uploadBuffer.data(), outEditCount * sizeof(SDFEditGPU));
    }
}

} // namespace Astral
