#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/BrickGrid.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/QualitySettings.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"
#include "Astral/Geometry/SDFChangeSet.hpp"

#include <span>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <glm/gtc/quaternion.hpp>

namespace Astral {

BrickGrid::BrickGrid(vk::Device device, vk::PhysicalDevice physicalDevice)
    : BrickGrid(VK_NULL_HANDLE, device, physicalDevice) {}

BrickGrid::BrickGrid(VmaAllocator allocator, vk::Device device, vk::PhysicalDevice physicalDevice)
    : m_Device(device),
      m_PhysicalDevice(physicalDevice) {

    m_CellSize = (m_MaxBounds - m_MinBounds) / glm::vec3(
        static_cast<float>(DIM_X),
        static_cast<float>(DIM_Y),
        static_cast<float>(DIM_Z)
    );

    m_CellDistances.resize(TOTAL_CELLS, 10.0f);

    if (allocator != VK_NULL_HANDLE || m_Device) {
        vk::DeviceSize bufferSize = sizeof(GridGPUHeader) + TOTAL_CELLS * sizeof(float);
        m_GridBuffer = std::make_unique<Buffer>(
            allocator,
            m_Device,
            m_PhysicalDevice,
            bufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true // Persistent Mapping aktif
        );

        UploadGridBuffer();

        std::cout << "[Astral::BrickGrid] Two-Level Spatial Grid baslatildi (" 
                  << DIM_X << "x" << DIM_Y << "x" << DIM_Z 
                  << ", Hucre boyutu: " << m_CellSize.x << "m, Tampon: " << bufferSize / 1024 << " KB, VMA: "
                  << (allocator != VK_NULL_HANDLE ? "Evet" : "Hayir") << ").\n";
    }
}

BrickGrid::~BrickGrid() {
    m_GridBuffer.reset();
}

void BrickGrid::Build(std::span<const LegacySDFEdit> edits) {
    std::vector<SDFPrimitiveRecord> records;
    records.reserve(edits.size());
    for (size_t i = 0; i < edits.size(); ++i) {
        records.push_back(edits[i].ToPrimitiveRecord(static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1)));
    }
    Build(std::span<const SDFPrimitiveRecord>(records));
}

void BrickGrid::UploadGridBuffer() {
    if (!m_GridBuffer) return;

    GridGPUHeader header{};
    header.gridMinBounds = glm::vec4(m_MinBounds, 0.0f);
    header.gridMaxBounds = glm::vec4(m_MaxBounds, 0.0f);
    header.gridParams = glm::vec4(
        static_cast<float>(DIM_X),
        static_cast<float>(DIM_Y),
        static_cast<float>(DIM_Z),
        m_CellSize.x
    );

    m_GridBuffer->UpdateData(&header, sizeof(GridGPUHeader), 0);
    if (!m_CellDistances.empty()) {
        m_GridBuffer->UpdateData(m_CellDistances.data(), m_CellDistances.size() * sizeof(float), sizeof(GridGPUHeader));
    }
}

void BrickGrid::SetBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds) {
    m_MinBounds = minBounds;
    m_MaxBounds = maxBounds;
    m_CellSize = (m_MaxBounds - m_MinBounds) / glm::vec3(
        static_cast<float>(DIM_X),
        static_cast<float>(DIM_Y),
        static_cast<float>(DIM_Z)
    );
}

bool BrickGrid::ComputeDynamicBounds(std::span<const SDFPrimitiveRecord> records, float margin, bool forceFit) {
    QualitySettings defaultQs{};
    glm::vec3 defaultMin = defaultQs.defaultGridMin;
    glm::vec3 defaultMax = defaultQs.defaultGridMax;

    glm::vec3 sceneMin( 1e9f);
    glm::vec3 sceneMax(-1e9f);
    bool hasFinitePrimitives = false;

    for (const auto& rec : records) {
        if (rec.primitiveType == 3) continue; // Skip infinite planes

        glm::mat4 worldFromLocal = glm::inverse(rec.invTransform);
        glm::vec3 center = glm::vec3(worldFromLocal[3]);
        glm::vec3 localExtents = glm::abs(glm::vec3(rec.dimensions));
        glm::vec3 col0 = glm::vec3(worldFromLocal[0]);
        glm::vec3 col1 = glm::vec3(worldFromLocal[1]);
        glm::vec3 col2 = glm::vec3(worldFromLocal[2]);
        glm::vec3 worldExtents = glm::abs(col0) * localExtents.x +
                                 glm::abs(col1) * localExtents.y +
                                 glm::abs(col2) * localExtents.z;

        sceneMin = glm::min(sceneMin, center - worldExtents);
        sceneMax = glm::max(sceneMax, center + worldExtents);
        hasFinitePrimitives = true;
    }

    glm::vec3 targetMin = defaultMin;
    glm::vec3 targetMax = defaultMax;

    if (hasFinitePrimitives) {
        if (forceFit) {
            targetMin = sceneMin - glm::vec3(margin);
            targetMax = sceneMax + glm::vec3(margin);
        } else {
            targetMin = glm::min(defaultMin, sceneMin - glm::vec3(margin));
            targetMax = glm::max(defaultMax, sceneMax + glm::vec3(margin));
        }
    }

    if (glm::distance(targetMin, m_MinBounds) > 0.05f || glm::distance(targetMax, m_MaxBounds) > 0.05f) {
        SetBounds(targetMin, targetMax);
        return true;
    }
    return false;
}

namespace {

struct RecordWorldBound {
    uint32_t primitiveType;
    glm::vec3 center;
    float boundRadius;
    glm::mat4 invTransform;
    float planeOffset;
    float conservativeScale;
};

std::vector<RecordWorldBound> PrecomputeRecordBounds(std::span<const SDFPrimitiveRecord> records) {
    std::vector<RecordWorldBound> bounds;
    bounds.reserve(records.size());
    for (const auto& rec : records) {
        RecordWorldBound b;
        b.primitiveType = rec.primitiveType;
        b.invTransform = rec.invTransform;
        b.planeOffset = rec.dimensions.x;
        b.conservativeScale = rec.metallicParams.z > 1e-6f ? rec.metallicParams.z : 1.0f;
        if (rec.primitiveType != 3) {
            glm::mat4 worldFromLocal = glm::inverse(rec.invTransform);
            b.center = glm::vec3(worldFromLocal[3]);
            glm::vec3 localExtents = glm::abs(glm::vec3(rec.dimensions));
            glm::vec3 col0 = glm::vec3(worldFromLocal[0]);
            glm::vec3 col1 = glm::vec3(worldFromLocal[1]);
            glm::vec3 col2 = glm::vec3(worldFromLocal[2]);
            glm::vec3 worldExtents = glm::abs(col0) * localExtents.x +
                                     glm::abs(col1) * localExtents.y +
                                     glm::abs(col2) * localExtents.z;
            b.boundRadius = glm::length(worldExtents);
        } else {
            b.center = glm::vec3(0.0f);
            b.boundRadius = 1e6f;
        }
        bounds.push_back(b);
    }
    return bounds;
}

} // namespace

float BrickGrid::EvaluateCell(uint32_t x, uint32_t y, uint32_t z, std::span<const SDFPrimitiveRecord> records) const {
    float halfDiag = glm::length(m_CellSize) * 0.5f;
    glm::vec3 cellCenter = m_MinBounds + (glm::vec3(x, y, z) + 0.5f) * m_CellSize;

    float minDist = 1.0f + halfDiag;
    float smoothExpansion = 0.0f;
    for (const auto& rec : records) {
        if (rec.operation == 3 || rec.operation == 4) smoothExpansion += std::max(rec.metallicParams.y, 0.001f) * 0.25f;
    }

    for (const auto& rec : records) {
        if (rec.primitiveType == 3) {
            glm::vec4 lp4 = rec.invTransform * glm::vec4(cellCenter, 1.0f);
            float scale = rec.metallicParams.z > 1e-6f ? rec.metallicParams.z : 1.0f;
            float dp = (lp4.y + rec.dimensions.x) * scale;
            minDist = std::min(minDist, dp);
            continue;
        }

        glm::mat4 worldFromLocal = glm::inverse(rec.invTransform);
        glm::vec3 center = glm::vec3(worldFromLocal[3]);
        glm::vec3 localExtents = glm::abs(glm::vec3(rec.dimensions));
        glm::vec3 col0 = glm::vec3(worldFromLocal[0]);
        glm::vec3 col1 = glm::vec3(worldFromLocal[1]);
        glm::vec3 col2 = glm::vec3(worldFromLocal[2]);
        glm::vec3 worldExtents = glm::abs(col0) * localExtents.x +
                                 glm::abs(col1) * localExtents.y +
                                 glm::abs(col2) * localExtents.z;
        float boundRadius = glm::length(worldExtents);
        float distToCenter = glm::distance(cellCenter, center);
        float dShape = distToCenter - boundRadius;
        minDist = std::min(minDist, dShape);
    }

    return std::clamp(minDist - halfDiag - smoothExpansion, 0.0f, 1.0f);
}

void BrickGrid::FullRebuild(std::span<const SDFPrimitiveRecord> records) {
    auto bounds = PrecomputeRecordBounds(records);
    float halfDiag = glm::length(m_CellSize) * 0.5f;
    float smoothExpansion = 0.0f;
    for (const auto& rec : records) {
        if (rec.operation == 3 || rec.operation == 4) smoothExpansion += std::max(rec.metallicParams.y, 0.001f) * 0.25f;
    }

    for (uint32_t z = 0; z < DIM_Z; ++z) {
        for (uint32_t y = 0; y < DIM_Y; ++y) {
            for (uint32_t x = 0; x < DIM_X; ++x) {
                size_t index = x + DIM_X * (y + DIM_Y * z);
                glm::vec3 cellCenter = m_MinBounds + (glm::vec3(x, y, z) + 0.5f) * m_CellSize;
                float minDist = 1.0f + halfDiag;
                for (const auto& b : bounds) {
                    if (b.primitiveType == 3) {
                        glm::vec4 lp4 = b.invTransform * glm::vec4(cellCenter, 1.0f);
                        float dp = (lp4.y + b.planeOffset) * b.conservativeScale;
                        minDist = std::min(minDist, dp);
                        continue;
                    }
                    float distToCenter = glm::distance(cellCenter, b.center);
                    float dShape = distToCenter - b.boundRadius;
                    minDist = std::min(minDist, dShape);
                }
                m_CellDistances[index] = std::clamp(minDist - halfDiag - smoothExpansion, 0.0f, 1.0f);
            }
        }
    }

    UploadGridBuffer();
    m_LastUpdatedCellCount = TOTAL_CELLS;
    m_CachedRecords.assign(records.begin(), records.end());
    m_IsRecordsInitialized = true;
}

void BrickGrid::Build(std::span<const SDFPrimitiveRecord> records, const SDFChangeSet* changeSet) {
    // 0. Dinamik sinirlari guncelle (eger sinirlar degistiyse mecburen FullRebuild)
    bool boundsChanged = ComputeDynamicBounds(records);
    if (boundsChanged || !m_IsRecordsInitialized || records.size() != m_CachedRecords.size()) {
        FullRebuild(records);
        return;
    }

    // 1. ChangeSet kontrolu (varsa)
    if (changeSet) {
        if (changeSet->IsGlobalChange()) {
            FullRebuild(records);
            return;
        }
        if (!changeSet->HasChanges()) {
            m_LastUpdatedCellCount = 0;
            return;
        }
    }

    // 2. Hangi primitiflerin degistigini tespit et (Dirty Tracking)
    std::vector<size_t> dirtyIndices;
    if (changeSet) {
        bool hasGeometricChange = false;
        for (const auto& ch : changeSet->GetChanges()) {
            if (HasFlag(ch.changeType, SDFChangeType::TransformMotion) ||
                HasFlag(ch.changeType, SDFChangeType::ShapeOrCSGChange) ||
                HasFlag(ch.changeType, SDFChangeType::VisibilityOrRemoval) ||
                HasFlag(ch.changeType, SDFChangeType::VisibilityOrAddition)) {
                hasGeometricChange = true;
                for (size_t i = 0; i < records.size(); ++i) {
                    if (records[i].surfaceId == ch.surfaceId) {
                        dirtyIndices.push_back(i);
                        break;
                    }
                }
            }
        }
        if (!hasGeometricChange) {
            // Yalnizca materyal degisikligi (albedo, roughness, metallic) — mesafe alani etkilenmez!
            m_LastUpdatedCellCount = 0;
            m_CachedRecords.assign(records.begin(), records.end());
            return;
        }
    } else {
        dirtyIndices.reserve(records.size());
        for (size_t i = 0; i < records.size(); ++i) {
            const auto& cur = records[i];
            const auto& prev = m_CachedRecords[i];

            if (cur.invTransform != prev.invTransform ||
                cur.dimensions != prev.dimensions ||
                cur.primitiveType != prev.primitiveType ||
                cur.operation != prev.operation ||
                cur.metallicParams.y != prev.metallicParams.y) {
                dirtyIndices.push_back(i);
            }
        }
    }

    // 3. Hicbir primitif degismediyse: EARLY-OUT (0 hucre islenir, GPU memcpy yapilmaz!)
    if (dirtyIndices.empty()) {
        m_LastUpdatedCellCount = 0;
        m_CachedRecords.assign(records.begin(), records.end());
        return;
    }

    // 4. Sonsuz etki alanli bir duzlem degistiyse veya CSG degisimi olduysa tam rebuild gerekir
    bool requiresFullRebuild = false;
    for (size_t idx : dirtyIndices) {
        if (records[idx].primitiveType == 3 || m_CachedRecords[idx].primitiveType == 3 ||
            records[idx].operation != m_CachedRecords[idx].operation ||
            records[idx].metallicParams.y != m_CachedRecords[idx].metallicParams.y) {
            requiresFullRebuild = true;
            break;
        }
    }

    if (requiresFullRebuild) {
        FullRebuild(records);
        return;
    }

    // 5. Kismi AABB Guncellemesi: Sadece degisen primitiflerin etki alanindaki hucreleri belirle
    std::vector<uint8_t> dirtyCells(TOTAL_CELLS, 0);
    std::vector<size_t> dirtyCellIndices;

    float cellDiag = glm::length(m_CellSize);
    float halfDiag = cellDiag * 0.5f;
    float smoothExpansion = 0.0f;
    for (const auto& rec : records) {
        if (rec.operation == 3 || rec.operation == 4) smoothExpansion += std::max(rec.metallicParams.y, 0.001f) * 0.25f;
    }

    for (size_t idx : dirtyIndices) {
        const auto& cur = records[idx];
        const auto& prev = m_CachedRecords[idx];

        glm::mat4 curWorld = glm::inverse(cur.invTransform);
        glm::mat4 prevWorld = glm::inverse(prev.invTransform);
        glm::vec3 curCenter = glm::vec3(curWorld[3]);
        glm::vec3 prevCenter = glm::vec3(prevWorld[3]);

        glm::vec3 curLocal = glm::abs(glm::vec3(cur.dimensions));
        glm::vec3 curExtents = glm::abs(glm::vec3(curWorld[0])) * curLocal.x +
                               glm::abs(glm::vec3(curWorld[1])) * curLocal.y +
                               glm::abs(glm::vec3(curWorld[2])) * curLocal.z;
        float curRadius = glm::length(curExtents) + smoothExpansion;

        glm::vec3 prevLocal = glm::abs(glm::vec3(prev.dimensions));
        glm::vec3 prevExtents = glm::abs(glm::vec3(prevWorld[0])) * prevLocal.x +
                                glm::abs(glm::vec3(prevWorld[1])) * prevLocal.y +
                                glm::abs(glm::vec3(prevWorld[2])) * prevLocal.z;
        float prevRadius = glm::length(prevExtents) + smoothExpansion;

        float margin = cellDiag * 1.5f + std::max(cur.metallicParams.y, prev.metallicParams.y);

        glm::vec3 minBound = glm::min(curCenter - glm::vec3(curRadius + margin),
                                      prevCenter - glm::vec3(prevRadius + margin));
        glm::vec3 maxBound = glm::max(curCenter + glm::vec3(curRadius + margin),
                                      prevCenter + glm::vec3(prevRadius + margin));

        int minX = std::clamp(static_cast<int>(std::floor((minBound.x - m_MinBounds.x) / m_CellSize.x)), 0, static_cast<int>(DIM_X - 1));
        int maxX = std::clamp(static_cast<int>(std::ceil((maxBound.x - m_MinBounds.x) / m_CellSize.x)), 0, static_cast<int>(DIM_X - 1));
        int minY = std::clamp(static_cast<int>(std::floor((minBound.y - m_MinBounds.y) / m_CellSize.y)), 0, static_cast<int>(DIM_Y - 1));
        int maxY = std::clamp(static_cast<int>(std::ceil((maxBound.y - m_MinBounds.y) / m_CellSize.y)), 0, static_cast<int>(DIM_Y - 1));
        int minZ = std::clamp(static_cast<int>(std::floor((minBound.z - m_MinBounds.z) / m_CellSize.z)), 0, static_cast<int>(DIM_Z - 1));
        int maxZ = std::clamp(static_cast<int>(std::ceil((maxBound.z - m_MinBounds.z) / m_CellSize.z)), 0, static_cast<int>(DIM_Z - 1));

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    size_t cIdx = static_cast<size_t>(x) + DIM_X * (static_cast<size_t>(y) + DIM_Y * static_cast<size_t>(z));
                    if (dirtyCells[cIdx] == 0) {
                        dirtyCells[cIdx] = 1;
                        dirtyCellIndices.push_back(cIdx);
                    }
                }
            }
        }
    }

    // 6. Sadece isaretlenen dirty hucreleri yeniden hesapla
    auto bounds = PrecomputeRecordBounds(records);
    for (size_t cIdx : dirtyCellIndices) {
        const uint32_t x = static_cast<uint32_t>(cIdx % DIM_X);
        const size_t yz = cIdx / DIM_X;
        const uint32_t y = static_cast<uint32_t>(yz % DIM_Y);
        const uint32_t z = static_cast<uint32_t>(yz / DIM_Y);

        glm::vec3 cellCenter = m_MinBounds + (glm::vec3(x, y, z) + 0.5f) * m_CellSize;
        float minDist = 1.0f + halfDiag;
        for (const auto& b : bounds) {
            if (b.primitiveType == 3) {
                glm::vec4 lp4 = b.invTransform * glm::vec4(cellCenter, 1.0f);
                float dp = (lp4.y + b.planeOffset) * b.conservativeScale;
                minDist = std::min(minDist, dp);
                continue;
            }
            float distToCenter = glm::distance(cellCenter, b.center);
            float dShape = distToCenter - b.boundRadius;
            minDist = std::min(minDist, dShape);
        }
        m_CellDistances[cIdx] = std::clamp(minDist - halfDiag - smoothExpansion, 0.0f, 1.0f);
    }

    UploadGridBuffer();
    m_LastUpdatedCellCount = dirtyCellIndices.size();
    m_CachedRecords.assign(records.begin(), records.end());
}

void BrickGrid::Build(const SDFSceneSnapshot& snapshot, const SDFChangeSet* changeSet) {
    Build(std::span<const SDFPrimitiveRecord>(snapshot.GetRecords().data(), snapshot.GetRecordCount()), changeSet);
}

} // namespace Astral
