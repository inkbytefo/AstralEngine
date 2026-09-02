#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/BrickGrid.hpp"
#include "Astral/Renderer/Buffer.hpp"

#include <cmath>
#include <algorithm>
#include <iostream>

namespace Astral {

BrickGrid::BrickGrid(vk::Device device, vk::PhysicalDevice physicalDevice)
    : m_Device(device),
      m_PhysicalDevice(physicalDevice) {

    m_CellSize = (m_MaxBounds - m_MinBounds) / glm::vec3(
        static_cast<float>(DIM_X),
        static_cast<float>(DIM_Y),
        static_cast<float>(DIM_Z)
    );

    m_CellDistances.resize(TOTAL_CELLS, 10.0f);

    vk::DeviceSize bufferSize = TOTAL_CELLS * sizeof(float); // 64 KB
    m_GridBuffer = std::make_unique<Buffer>(
        m_Device,
        m_PhysicalDevice,
        bufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        true // Persistent Mapping aktif
    );

    std::cout << "[Astral::BrickGrid] Two-Level Spatial Grid baslatildi (" 
              << DIM_X << "x" << DIM_Y << "x" << DIM_Z 
              << ", Hucre boyutu: " << m_CellSize.x << "m, Tampon: " << bufferSize / 1024 << " KB).\n";
}

BrickGrid::~BrickGrid() {
    m_GridBuffer.reset();
}

void BrickGrid::Build(const std::vector<SDFEditGPU>& edits) {
    float halfDiag = glm::length(m_CellSize) * 0.5f;

    // Her izgara hucresi icin muhafazakar mesafe (conservative distance) hesabi
    for (uint32_t z = 0; z < DIM_Z; ++z) {
        for (uint32_t y = 0; y < DIM_Y; ++y) {
            for (uint32_t x = 0; x < DIM_X; ++x) {
                glm::vec3 cellCenter = m_MinBounds + (glm::vec3(x, y, z) + 0.5f) * m_CellSize;

                // 1. Zemin mesafesi
                float minDist = cellCenter.y - (-1.0f);

                // 2. Sahnedeki tum primitiflere olan muhafazakar mesafe
                for (const auto& e : edits) {
                    // Primitif tipi 3 Plane ise ayri hesapla
                    if (e.primitiveType == 3) {
                        float dp = cellCenter.y - e.position.y;
                        minDist = std::min(minDist, dp);
                        continue;
                    }

                    float boundRadius = glm::length(e.scale) * 1.15f;
                    float distToCenter = glm::distance(cellCenter, e.position);
                    float dShape = distToCenter - boundRadius;

                    minDist = std::min(minDist, dShape);
                }

                // Hucre hacmini kapsayacak muhafazakar alt sinir
                float conservativeDist = std::max(minDist - halfDiag, 0.0f);

                size_t index = x + DIM_X * (y + DIM_Y * z);
                m_CellDistances[index] = conservativeDist;
            }
        }
    }

    // GPU Tamponuna kalici pointer uzerinden tek seferde memcpy
    m_GridBuffer->UpdateData(m_CellDistances.data(), m_CellDistances.size() * sizeof(float));
}

} // namespace Astral
