#pragma once

#include "Astral/Render/Renderer.hpp"
#include "Detail/GpuTypes.hpp"
#include "Graph/RenderGraph.hpp"
#include "Vk/Pipelines.hpp"
#include "Vk/Resources.hpp"

#include <array>
#include <span>
#include <vector>

namespace Astral::Render {

// Sparse brick volume: a baked, cached copy of the SDF shape list.
//
//   sparse map : one R32_UINT per cell, either BRICK_FLAG | brickId or the float bits
//                of a conservative "safe skip" distance for an empty cell.
//   atlas      : R16F 3D texture of 8^3-sample bricks; a brick covers one cell and stores
//                its own border samples, so trilinear filtering never crosses bricks.
//
// Shapes that change mark world-space boxes dirty; only those cells are re-baked.
class SdfVolume {
public:
    static constexpr uint32_t kMaxShapes = 1024;
    static constexpr uint32_t kFrames = 2;

    SdfVolume(Vk::Device& device, const SdfVolumeSettings& settings);

    // Compares the new shape list with what is baked and marks changed regions dirty.
    void SetShapes(std::span<const Detail::GpuSdfShape> shapes);
    void MarkAllDirty();
    bool NeedsBake() const { return m_Dirty; }

    // Records the bake passes. `shapeBuffer` holds the shapes passed to SetShapes.
    // Also refreshes this frame slot's parameter buffer; call every frame.
    void RecordBake(RenderGraph& graph, vk::CommandBuffer cmd, uint32_t frameSlot, vk::Buffer shapeBuffer);
    void CollectStats(uint32_t frameSlot); // after the slot's fence

    Vk::Image& Atlas() { return m_Atlas; }
    Vk::Image& Map() { return m_Map; }
    vk::Buffer ParamsBuffer(uint32_t frameSlot) const { return m_Params[frameSlot].Get(); }
    uint32_t ShapeCount() const { return static_cast<uint32_t>(m_Baked.size()); }

    uint32_t Capacity() const { return m_Capacity; }
    uint32_t Used() const { return m_Used; }
    uint32_t Dropped() const { return m_Dropped; }
    uint32_t LastBakeCells() const { return m_LastBakeCells; }

private:
    void MarkDirty(const glm::vec3& min, const glm::vec3& max);
    void UpdateParams(uint32_t frameSlot, uint32_t shapeCount);
    float CellSize() const { return m_Settings.voxelSize * 7.0f; }

    struct Push {
        glm::uvec4 boxMin;  // w = pass
        glm::uvec4 boxSize;
    };

    Vk::Device& m_Device;
    SdfVolumeSettings m_Settings;
    uint32_t m_Capacity = 0;
    Detail::SdfVolumeParams m_ParamsData{};

    Vk::Image m_Atlas;
    Vk::Image m_Map;
    Vk::Buffer m_State;   // freeTop, overflow, allocCount, pad, freeStack[capacity]
    Vk::Buffer m_Alloc;
    Vk::Buffer m_Work;    // dispatch-indirect header + (cell, brick) pairs
    std::array<Vk::Buffer, kFrames> m_Params; // per frame: the CPU rewrites it every frame
    std::array<Vk::Buffer, kFrames> m_Stats;
    std::array<bool, kFrames> m_StatsPending{};
    BufferState m_StateSync, m_AllocSync, m_WorkSync;
    std::array<BufferState, kFrames> m_StatsSync;
    Vk::Pipeline m_Pipeline;

    std::vector<Detail::GpuSdfShape> m_Baked;
    bool m_Dirty = false;
    glm::ivec3 m_DirtyMin{0}, m_DirtyMax{0};
    uint32_t m_Used = 0, m_Dropped = 0, m_LastBakeCells = 0;
};

} // namespace Astral::Render
