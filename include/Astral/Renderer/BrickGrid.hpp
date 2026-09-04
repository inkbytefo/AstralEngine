#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <span>
#include <memory>
#include "Astral/Renderer/SDFEdit.hpp"

namespace Astral {

class Buffer;

/// Two-Level Acceleration Structure: Coarse 3D Spatial Grid (Empty Space Skipping).
/// RENDERER_ARCHITECTURE.md Bolum c.1 ve Bolum g.3.
class BrickGrid {
public:
    static constexpr uint32_t DIM_X = 32;
    static constexpr uint32_t DIM_Y = 16;
    static constexpr uint32_t DIM_Z = 32;
    static constexpr size_t TOTAL_CELLS = DIM_X * DIM_Y * DIM_Z; // 16,384 hucre

    BrickGrid(vk::Device device, vk::PhysicalDevice physicalDevice);
    ~BrickGrid();

    BrickGrid(const BrickGrid&) = delete;
    BrickGrid& operator=(const BrickGrid&) = delete;

    /// Sahnedeki primitiflerin AABB / yaricaplarina gore 3D izgarayi gunceller
    void Build(std::span<const SDFEditGPU> edits);

    /// Son Build cagrisinda kac hucrenin yeniden degerlendirildigini dondurur (test ve profil icin)
    size_t GetLastUpdatedCellCount() const { return m_LastUpdatedCellCount; }

    Buffer* GetBuffer() const { return m_GridBuffer.get(); }
    glm::vec3 GetMinBounds() const { return m_MinBounds; }
    glm::vec3 GetMaxBounds() const { return m_MaxBounds; }
    glm::vec3 GetCellSize() const { return m_CellSize; }
    glm::vec4 GetGridParams() const {
        return glm::vec4(static_cast<float>(DIM_X), static_cast<float>(DIM_Y), static_cast<float>(DIM_Z), m_CellSize.x);
    }

private:
    float EvaluateCell(uint32_t x, uint32_t y, uint32_t z, std::span<const SDFEditGPU> edits) const;
    void FullRebuild(std::span<const SDFEditGPU> edits);

    vk::Device m_Device;
    vk::PhysicalDevice m_PhysicalDevice;

    glm::vec3 m_MinBounds{-12.0f, -1.0f, -12.0f};
    glm::vec3 m_MaxBounds{ 12.0f, 11.0f,  12.0f};
    glm::vec3 m_CellSize{0.75f, 0.75f, 0.75f};

    std::unique_ptr<Buffer> m_GridBuffer;
    std::vector<float> m_CellDistances;
    std::vector<SDFEditGPU> m_CachedEdits;
    bool m_IsInitialized = false;
    size_t m_LastUpdatedCellCount = 0;
};

} // namespace Astral
