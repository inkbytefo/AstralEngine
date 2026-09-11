#include "Astral/Renderer/SceneGpuData.hpp"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Geometry/SDFChangeSet.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

namespace Astral {

SceneGpuData::SceneGpuData(VulkanContext& context, bool persistentMap)
    : m_Context(&context) {
    auto allocator = context.GetAllocator();
    auto device = context.GetDevice();
    auto physicalDevice = context.GetPhysicalDevice();

    // 1. Edit Buffer (MAX_SDF_EDITS * sizeof(SDFPrimitiveRecord))
    vk::DeviceSize editBufferSize = MAX_SDF_EDITS * sizeof(SDFPrimitiveRecord);
    m_EditBuffer = std::make_unique<Buffer>(
        allocator, device, physicalDevice,
        editBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        persistentMap
    );

    // 2. Previous Transform Buffer (MAX_SDF_EDITS * sizeof(glm::mat4))
    vk::DeviceSize prevBufferSize = MAX_SDF_EDITS * sizeof(glm::mat4);
    m_PrevTransformBuffer = std::make_unique<Buffer>(
        allocator, device, physicalDevice,
        prevBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        persistentMap
    );

    // 3. Light Buffer (sizeof(LightBufferHeader) + MAX_LIGHTS * sizeof(LightGPU))
    vk::DeviceSize lightBufferSize = sizeof(LightBufferHeader) + MAX_LIGHTS * sizeof(LightGPU);
    m_LightBuffer = std::make_unique<Buffer>(
        allocator, device, physicalDevice,
        lightBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        true
    );

    // Varsayilan isiklar: 1 Yonlu (Gunes) + 1 Noktasal (Dolgu)
    m_Lights.clear();
    LightGPU sunLight{};
    sunLight.position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Directional
    sunLight.direction = glm::vec4(glm::normalize(glm::vec3(0.5f, -0.8f, 0.4f)), 3.0f);
    sunLight.color = glm::vec4(1.0f, 0.95f, 0.88f, 0.0f);
    m_Lights.push_back(sunLight);

    LightGPU fillLight{};
    fillLight.position = glm::vec4(-2.0f, 2.5f, 2.0f, 1.0f); // Point
    fillLight.direction = glm::vec4(0.0f, 0.0f, 0.0f, 2.0f);
    fillLight.color = glm::vec4(0.4f, 0.65f, 1.0f, 20.0f);
    m_Lights.push_back(fillLight);

    UpdateLights();

    // 4. BrickGrid
    m_BrickGrid = std::make_unique<BrickGrid>(allocator, device, physicalDevice);

    // 5. Camera UBO
    m_CameraUBO = std::make_unique<Buffer>(
        allocator, device, physicalDevice,
        sizeof(CameraUBOData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        true
    );
    if (m_CameraUBO->GetMappedData()) {
        CameraUBOData initData{};
        std::memcpy(m_CameraUBO->GetMappedData(), &initData, sizeof(CameraUBOData));
    }
}

SceneGpuData::~SceneGpuData() = default;

SceneGpuData::SceneGpuData(SceneGpuData&&) noexcept = default;
SceneGpuData& SceneGpuData::operator=(SceneGpuData&&) noexcept = default;

void SceneGpuData::Upload(std::span<const SDFPrimitiveRecord> records, const SDFChangeSet& changeSet, bool legacyMap) {
    m_ActiveEditCount = static_cast<uint32_t>(std::min(records.size(), MAX_SDF_EDITS));
    if (m_ActiveEditCount > 0) {
        size_t uploadBytes = m_ActiveEditCount * sizeof(SDFPrimitiveRecord);
        if (legacyMap) {
            m_EditBuffer->UpdateDataLegacy(records.data(), uploadBytes);
        } else {
            m_EditBuffer->UpdateData(records.data(), uploadBytes);
        }

        if (m_PrevTransformBuffer) {
            std::vector<glm::mat4> prevMatrices(m_ActiveEditCount);
            m_CandidateWorldTransforms.clear();
            m_CandidateWorldTransforms.reserve(m_ActiveEditCount);

            for (size_t i = 0; i < m_ActiveEditCount; ++i) {
                const auto& rec = records[i];
                glm::mat4 currWorldTransform = glm::inverse(rec.invTransform);
                // For valid entity surfaceId, use rec.surfaceId. If 0 (e.g. raw test record), fallback to unique per-index key.
                uint32_t key = (rec.surfaceId != 0u) ? rec.surfaceId : MakeFallbackTransformKey(static_cast<uint32_t>(i));
                auto it = m_PrevWorldTransforms.find(key);
                if (it != m_PrevWorldTransforms.end()) {
                    prevMatrices[i] = it->second;
                } else {
                    prevMatrices[i] = currWorldTransform;
                }
                m_CandidateWorldTransforms[key] = currWorldTransform;
            }

            size_t prevUploadBytes = m_ActiveEditCount * sizeof(glm::mat4);
            if (legacyMap) {
                m_PrevTransformBuffer->UpdateDataLegacy(prevMatrices.data(), prevUploadBytes);
            } else {
                m_PrevTransformBuffer->UpdateData(prevMatrices.data(), prevUploadBytes);
            }
        }
    } else {
        m_PrevWorldTransforms.clear();
        m_CandidateWorldTransforms.clear();
    }

    if (m_BrickGrid) {
        m_BrickGrid->Build(records.subspan(0, m_ActiveEditCount), &changeSet);
    }
}

SceneBufferViews SceneGpuData::GetViews() const {
    SceneBufferViews views{};
    views.primitives = m_EditBuffer ? m_EditBuffer->GetDescriptorInfo() : vk::DescriptorBufferInfo{};
    views.previousTransforms = m_PrevTransformBuffer ? m_PrevTransformBuffer->GetDescriptorInfo() : vk::DescriptorBufferInfo{};
    views.lights = m_LightBuffer ? m_LightBuffer->GetDescriptorInfo() : vk::DescriptorBufferInfo{};
    views.grid = (m_BrickGrid && m_BrickGrid->GetBuffer()) ? m_BrickGrid->GetBuffer()->GetDescriptorInfo() : vk::DescriptorBufferInfo{};
    views.primitiveCount = m_ActiveEditCount;
    views.gridParams = m_BrickGrid ? m_BrickGrid->GetGridParams() : glm::vec4(0.0f);
    return views;
}

void SceneGpuData::SetLights(std::span<const LightGPU> lights) {
    m_Lights.assign(lights.begin(), lights.end());
    UpdateLights();
}

void SceneGpuData::UpdateLights() {
    if (!m_LightBuffer) return;

    LightBufferHeader header{};
    header.lightCount = static_cast<uint32_t>(std::min(m_Lights.size(), MAX_LIGHTS));
    m_LightBuffer->UpdateData(&header, sizeof(LightBufferHeader), 0);

    if (!m_Lights.empty()) {
        size_t uploadCount = std::min(m_Lights.size(), MAX_LIGHTS);
        m_LightBuffer->UpdateData(m_Lights.data(), uploadCount * sizeof(LightGPU), sizeof(LightBufferHeader));
    }
}

void SceneGpuData::UploadCamera(const CameraUBOData& cameraData) {
    if (m_CameraUBO && m_CameraUBO->GetMappedData()) {
        std::memcpy(m_CameraUBO->GetMappedData(), &cameraData, sizeof(CameraUBOData));
    }
}

vk::DescriptorBufferInfo SceneGpuData::GetCameraDescriptor() const {
    return m_CameraUBO ? m_CameraUBO->GetDescriptorInfo() : vk::DescriptorBufferInfo{};
}

void SceneGpuData::CommitSubmitted() noexcept {
    if (!m_CandidateWorldTransforms.empty()) {
        m_PrevWorldTransforms = std::move(m_CandidateWorldTransforms);
        m_CandidateWorldTransforms.clear();
    }
}

void SceneGpuData::AbortPrepared() noexcept {
    m_CandidateWorldTransforms.clear();
}

void SceneGpuData::ResetTransformHistory() noexcept {
    m_PrevWorldTransforms.clear();
    m_CandidateWorldTransforms.clear();
}

} // namespace Astral
