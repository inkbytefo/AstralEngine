#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Components.hpp"
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
        if (registry.HasComponent<SDFComponent>(entity)) {
            const auto& sdf = registry.GetComponent<SDFComponent>(entity);

            SDFEditGPU gpuData{};
            gpuData.position = transform.position;
            gpuData.rotation = glm::vec4(
                transform.rotation.x,
                transform.rotation.y,
                transform.rotation.z,
                transform.rotation.w
            );
            gpuData.scale = transform.scale;
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
