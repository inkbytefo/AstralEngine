#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/BrickGrid.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Geometry/SDFSceneSnapshot.hpp"

#include <cmath>
#include <algorithm>
#include <iostream>
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
        vk::DeviceSize bufferSize = TOTAL_CELLS * sizeof(float); // 64 KB
        m_GridBuffer = std::make_unique<Buffer>(
            allocator,
            m_Device,
            m_PhysicalDevice,
            bufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            true // Persistent Mapping aktif
        );

        std::cout << "[Astral::BrickGrid] Two-Level Spatial Grid baslatildi (" 
                  << DIM_X << "x" << DIM_Y << "x" << DIM_Z 
                  << ", Hucre boyutu: " << m_CellSize.x << "m, Tampon: " << bufferSize / 1024 << " KB, VMA: "
                  << (allocator != VK_NULL_HANDLE ? "Evet" : "Hayir") << ").\n";
    }
}

BrickGrid::~BrickGrid() {
    m_GridBuffer.reset();
}

float BrickGrid::EvaluateCell(uint32_t x, uint32_t y, uint32_t z, std::span<const SDFEditGPU> edits) const {
    float halfDiag = glm::length(m_CellSize) * 0.5f;
    glm::vec3 cellCenter = m_MinBounds + (glm::vec3(x, y, z) + 0.5f) * m_CellSize;

    // 1. Zemin mesafesi
    float minDist = 1.0f + halfDiag;
    float smoothExpansion = 0.0f;
    for (const auto& edit : edits)
        if (edit.operation == 3) smoothExpansion += std::max(edit.blendFactor, 0.001f) * 0.25f;

    // 2. Sahnedeki tum primitiflere olan muhafazakar mesafe
    for (const auto& e : edits) {
        // Primitif tipi 3 Plane ise ayri hesapla
        if (e.primitiveType == 3) {
            glm::quat q(e.rotation.w, e.rotation.x, e.rotation.y, e.rotation.z);
            q = glm::dot(q, q) > 0.001f ? glm::normalize(q) : glm::quat(1, 0, 0, 0);
            float dp = (glm::conjugate(q) * (cellCenter - e.position)).y + e.scale.y;
            minDist = std::min(minDist, dp);
            continue;
        }

        float boundRadius = glm::dot(glm::abs(e.scale), glm::vec3(1.0f));
        float distToCenter = glm::distance(cellCenter, e.position);
        float dShape = distToCenter - boundRadius;

        minDist = std::min(minDist, dShape);
    }

    return std::clamp(minDist - halfDiag - smoothExpansion, 0.0f, 1.0f);
}

void BrickGrid::FullRebuild(std::span<const SDFEditGPU> edits) {
    for (uint32_t z = 0; z < DIM_Z; ++z) {
        for (uint32_t y = 0; y < DIM_Y; ++y) {
            for (uint32_t x = 0; x < DIM_X; ++x) {
                size_t index = x + DIM_X * (y + DIM_Y * z);
                m_CellDistances[index] = EvaluateCell(x, y, z, edits);
            }
        }
    }

    // GPU Tamponuna kalici pointer uzerinden tek seferde memcpy
    if (m_GridBuffer) {
        m_GridBuffer->UpdateData(m_CellDistances.data(), m_CellDistances.size() * sizeof(float));
    }
    m_LastUpdatedCellCount = TOTAL_CELLS;
}

void BrickGrid::Build(std::span<const SDFEditGPU> edits) {
    // 1. Ilk calisma veya primitif sayisi degisikligi (ekleme / silme)
    if (!m_IsInitialized || edits.size() != m_CachedEdits.size()) {
        FullRebuild(edits);
        m_CachedEdits.assign(edits.begin(), edits.end());
        m_IsInitialized = true;
        return;
    }

    // 2. Hangi primitiflerin degistigini tespit et (Dirty Tracking)
    std::vector<size_t> dirtyIndices;
    dirtyIndices.reserve(edits.size());

    for (size_t i = 0; i < edits.size(); ++i) {
        const auto& cur = edits[i];
        const auto& prev = m_CachedEdits[i];

        // Sadece mesafe alanini (SDF distance) etkileyen ozellikleri karsilastir
        // (Albedo, roughness, metallic gibi materyal ozellikleri grid mesafesini degistirmez)
        if (cur.position != prev.position ||
            cur.rotation != prev.rotation ||
            cur.scale != prev.scale ||
            cur.primitiveType != prev.primitiveType ||
            cur.operation != prev.operation ||
            cur.blendFactor != prev.blendFactor) {
            dirtyIndices.push_back(i);
        }
    }

    // 3. Hicbir primitif degismediyse: EARLY-OUT (0 hucre islenir, GPU memcpy yapilmaz!)
    if (dirtyIndices.empty()) {
        m_LastUpdatedCellCount = 0;
        return;
    }

    // 4. Yalniz sonsuz etki alanli bir duzlem degistiyse tam rebuild gerekir.
    // Buyuk hareketler eski ve yeni AABB'nin birlesimiyle dogal olarak daha cok hucreyi kirletir.
    bool requiresFullRebuild = false;
    for (size_t idx : dirtyIndices) {
        if (edits[idx].primitiveType == 3 || m_CachedEdits[idx].primitiveType == 3 || edits[idx].operation != m_CachedEdits[idx].operation || edits[idx].blendFactor != m_CachedEdits[idx].blendFactor) {
            requiresFullRebuild = true;
            break;
        }
    }

    if (requiresFullRebuild) {
        FullRebuild(edits);
        m_CachedEdits.assign(edits.begin(), edits.end());
        return;
    }

    // 5. Kısmi AABB Güncellemesi: Sadece degisen primitiflerin etki alanindaki hucreleri belirle
    std::vector<uint8_t> dirtyCells(TOTAL_CELLS, 0);
    std::vector<size_t> dirtyCellIndices;

    float cellDiag = glm::length(m_CellSize);
    float smoothExpansion = 0.0f;
    for (const auto& edit : edits)
        if (edit.operation == 3) smoothExpansion += std::max(edit.blendFactor, 0.001f) * 0.25f;

    for (size_t idx : dirtyIndices) {
        const auto& cur = edits[idx];
        const auto& prev = m_CachedEdits[idx];

        float curRadius = glm::dot(glm::abs(cur.scale), glm::vec3(1.0f)) + smoothExpansion;
        float prevRadius = glm::dot(glm::abs(prev.scale), glm::vec3(1.0f)) + smoothExpansion;

        // Guvenlik marjini: Hucre boyutu + blend factor + yer degistirme payi
        float margin = cellDiag * 1.5f + std::max(cur.blendFactor, prev.blendFactor);

        glm::vec3 minBound = glm::min(cur.position - glm::vec3(curRadius + margin),
                                      prev.position - glm::vec3(prevRadius + margin));
        glm::vec3 maxBound = glm::max(cur.position + glm::vec3(curRadius + margin),
                                      prev.position + glm::vec3(prevRadius + margin));

        // Dunya koordinatlarindan izgara hucre koordinatlarina donustur (clamp ile)
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
    for (size_t cIdx : dirtyCellIndices) {
        const uint32_t x = static_cast<uint32_t>(cIdx % DIM_X);
        const size_t yz = cIdx / DIM_X;
        const uint32_t y = static_cast<uint32_t>(yz % DIM_Y);
        const uint32_t z = static_cast<uint32_t>(yz / DIM_Y);
        m_CellDistances[cIdx] = EvaluateCell(x, y, z, edits);
    }

    // 7. Sirali hucreleri araliklar halinde GPU tamponuna aktar ve onbellegi esle.
    if (m_GridBuffer) {
        // Hucreler AABB dolasiminda sirali eklenmedigi icin once fiziksel tampon sirasina getir.
        std::sort(dirtyCellIndices.begin(), dirtyCellIndices.end());
        size_t rangeBegin = 0;
        while (rangeBegin < dirtyCellIndices.size()) {
            size_t rangeEnd = rangeBegin + 1;
            while (rangeEnd < dirtyCellIndices.size() &&
                   dirtyCellIndices[rangeEnd] == dirtyCellIndices[rangeEnd - 1] + 1) {
                ++rangeEnd;
            }

            const size_t firstCell = dirtyCellIndices[rangeBegin];
            const size_t cellCount = dirtyCellIndices[rangeEnd - 1] - firstCell + 1;
            m_GridBuffer->UpdateData(
                m_CellDistances.data() + firstCell,
                cellCount * sizeof(float),
                firstCell * sizeof(float)
            );
            rangeBegin = rangeEnd;
        }
    }

    m_CachedEdits.assign(edits.begin(), edits.end());
    m_LastUpdatedCellCount = dirtyCellIndices.size();
}

float BrickGrid::EvaluateCell(uint32_t x, uint32_t y, uint32_t z, std::span<const SDFPrimitiveRecord> records) const {
    float halfDiag = glm::length(m_CellSize) * 0.5f;
    glm::vec3 cellCenter = m_MinBounds + (glm::vec3(x, y, z) + 0.5f) * m_CellSize;

    float minDist = 1.0f + halfDiag;
    float smoothExpansion = 0.0f;
    for (const auto& rec : records) {
        if (rec.operation == 3) smoothExpansion += std::max(rec.metallicParams.y, 0.001f) * 0.25f;
    }

    for (const auto& rec : records) {
        if (rec.primitiveType == 3) {
            glm::vec4 lp4 = rec.invTransform * glm::vec4(cellCenter, 1.0f);
            float dp = lp4.y + rec.dimensions.x;
            minDist = std::min(minDist, dp);
            continue;
        }

        glm::mat4 worldFromLocal = glm::inverse(rec.invTransform);
        glm::vec3 center = glm::vec3(worldFromLocal[3]);
        float boundRadius = glm::dot(glm::abs(glm::vec3(rec.dimensions)), glm::vec3(1.0f));
        float distToCenter = glm::distance(cellCenter, center);
        float dShape = distToCenter - boundRadius;
        minDist = std::min(minDist, dShape);
    }

    return std::clamp(minDist - halfDiag - smoothExpansion, 0.0f, 1.0f);
}

void BrickGrid::FullRebuild(std::span<const SDFPrimitiveRecord> records) {
    for (uint32_t z = 0; z < DIM_Z; ++z) {
        for (uint32_t y = 0; y < DIM_Y; ++y) {
            for (uint32_t x = 0; x < DIM_X; ++x) {
                size_t index = x + DIM_X * (y + DIM_Y * z);
                m_CellDistances[index] = EvaluateCell(x, y, z, records);
            }
        }
    }

    if (m_GridBuffer) {
        m_GridBuffer->UpdateData(m_CellDistances.data(), m_CellDistances.size() * sizeof(float));
    }
    m_LastUpdatedCellCount = TOTAL_CELLS;
    m_CachedRecords.assign(records.begin(), records.end());
    m_IsRecordsInitialized = true;
}

void BrickGrid::Build(std::span<const SDFPrimitiveRecord> records) {
    // 1. Ilk calisma veya primitif sayisi degisikligi (ekleme / silme)
    if (!m_IsRecordsInitialized || records.size() != m_CachedRecords.size()) {
        FullRebuild(records);
        return;
    }

    // 2. Hangi primitiflerin degistigini tespit et (Dirty Tracking)
    std::vector<size_t> dirtyIndices;
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

    // 3. Hicbir primitif degismediyse: EARLY-OUT (0 hucre islenir, GPU memcpy yapilmaz!)
    if (dirtyIndices.empty()) {
        m_LastUpdatedCellCount = 0;
        return;
    }

    // 4. Yalniz sonsuz etki alanli bir duzlem degistiyse veya CSG degisimi olduysa tam rebuild gerekir
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
    float smoothExpansion = 0.0f;
    for (const auto& rec : records) {
        if (rec.operation == 3) smoothExpansion += std::max(rec.metallicParams.y, 0.001f) * 0.25f;
    }

    for (size_t idx : dirtyIndices) {
        const auto& cur = records[idx];
        const auto& prev = m_CachedRecords[idx];

        glm::mat4 curWorld = glm::inverse(cur.invTransform);
        glm::mat4 prevWorld = glm::inverse(prev.invTransform);
        glm::vec3 curCenter = glm::vec3(curWorld[3]);
        glm::vec3 prevCenter = glm::vec3(prevWorld[3]);

        float curRadius = glm::dot(glm::abs(glm::vec3(cur.dimensions)), glm::vec3(1.0f)) + smoothExpansion;
        float prevRadius = glm::dot(glm::abs(glm::vec3(prev.dimensions)), glm::vec3(1.0f)) + smoothExpansion;

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
    for (size_t cIdx : dirtyCellIndices) {
        const uint32_t x = static_cast<uint32_t>(cIdx % DIM_X);
        const size_t yz = cIdx / DIM_X;
        const uint32_t y = static_cast<uint32_t>(yz % DIM_Y);
        const uint32_t z = static_cast<uint32_t>(yz / DIM_Y);

        m_CellDistances[cIdx] = EvaluateCell(x, y, z, records);
    }

    if (m_GridBuffer) {
        m_GridBuffer->UpdateData(m_CellDistances.data(), m_CellDistances.size() * sizeof(float));
    }

    m_LastUpdatedCellCount = dirtyCellIndices.size();
    m_CachedRecords.assign(records.begin(), records.end());
}

void BrickGrid::Build(const SDFSceneSnapshot& snapshot) {
    Build(std::span<const SDFPrimitiveRecord>(snapshot.GetRecords().data(), snapshot.GetRecordCount()));
}

} // namespace Astral
