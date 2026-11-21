#include "SdfVolume.hpp"
#include "Vk/Device.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Astral::Render {

namespace {
constexpr uint32_t kPassInit = 0, kPassClassify = 1, kPassAllocate = 2, kPassFill = 3;
constexpr uint32_t kBrickSamples = 8;
constexpr uint32_t kMaxDispatchX = 65535;

const Vk::PipelineLayoutDesc kLayout{
    {
        {0, vk::DescriptorType::eStorageBuffer}, // shapes
        {1, vk::DescriptorType::eStorageImage},  // sparse map
        {2, vk::DescriptorType::eStorageImage},  // atlas
        {3, vk::DescriptorType::eStorageBuffer}, // state / free stack
        {4, vk::DescriptorType::eStorageBuffer}, // alloc list
        {5, vk::DescriptorType::eStorageBuffer}, // work list
        {6, vk::DescriptorType::eUniformBuffer}, // volume params
    },
    sizeof(glm::uvec4) * 2,
    false,
};
} // namespace

SdfVolume::SdfVolume(Vk::Device& device, const SdfVolumeSettings& settings) : m_Device(device), m_Settings(settings) {
    const glm::uvec3 atlas = settings.atlasBricks;
    m_Capacity = atlas.x * atlas.y * atlas.z;
    if (m_Capacity > kMaxDispatchX) throw std::runtime_error("SDF atlas capacity must not exceed 65535 bricks");

    const float cell = CellSize();
    m_ParamsData.originVoxel = glm::vec4(settings.origin, settings.voxelSize);
    m_ParamsData.gridDim = glm::uvec4(settings.cells, 0);
    m_ParamsData.atlasBricks = glm::uvec4(atlas, m_Capacity);
    m_ParamsData.metrics = glm::vec4(cell, 0.5f * cell * std::sqrt(3.0f), cell, 4.0f * cell);

    Vk::ImageDesc atlasDesc;
    atlasDesc.extent = vk::Extent3D{atlas.x * kBrickSamples, atlas.y * kBrickSamples, atlas.z * kBrickSamples};
    atlasDesc.format = vk::Format::eR16Sfloat;
    atlasDesc.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
    atlasDesc.is3D = true;
    atlasDesc.name = "SDF brick atlas";
    m_Atlas = Vk::Image(device, atlasDesc);

    Vk::ImageDesc mapDesc;
    mapDesc.extent = vk::Extent3D{settings.cells.x, settings.cells.y, settings.cells.z};
    mapDesc.format = vk::Format::eR32Uint;
    mapDesc.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
    mapDesc.is3D = true;
    mapDesc.name = "SDF sparse map";
    m_Map = Vk::Image(device, mapDesc);

    const auto storage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
    m_State = Vk::Buffer(device, 16 + 4ull * m_Capacity, storage | vk::BufferUsageFlagBits::eTransferSrc, Vk::MemoryUsage::GpuOnly, "SDF brick state");
    m_Alloc = Vk::Buffer(device, 4ull * m_Capacity, storage, Vk::MemoryUsage::GpuOnly, "SDF alloc list");
    m_Work = Vk::Buffer(device, 16 + 8ull * m_Capacity, storage | vk::BufferUsageFlagBits::eIndirectBuffer, Vk::MemoryUsage::GpuOnly, "SDF work list");
    for (auto& p : m_Params)
        p = Vk::Buffer(device, sizeof(Detail::SdfVolumeParams), vk::BufferUsageFlagBits::eUniformBuffer, Vk::MemoryUsage::Upload, "SDF params");
    for (auto& s : m_Stats) s = Vk::Buffer(device, 16, vk::BufferUsageFlagBits::eTransferDst, Vk::MemoryUsage::Readback, "SDF stats");
    for (uint32_t f = 0; f < kFrames; ++f) UpdateParams(f, 0);

    m_Pipeline = Vk::Pipeline::Compute(device, "SdfBake.comp", kLayout);

    // Every cell starts empty (safe distance 0) and every brick starts free.
    Vk::Buffer dummyShapes(device, sizeof(Detail::GpuSdfShape), vk::BufferUsageFlagBits::eStorageBuffer, Vk::MemoryUsage::Upload);
    device.ImmediateSubmit([&](vk::CommandBuffer cmd) {
        Vk::TransitionImage(cmd, m_Atlas, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
                            vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead);
        Vk::TransitionImage(cmd, m_Map, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eClear,
                            vk::AccessFlagBits2::eTransferWrite);
        cmd.clearColorImage(m_Map.Get(), vk::ImageLayout::eGeneral, vk::ClearColorValue(std::array<uint32_t, 4>{0, 0, 0, 0}),
                            m_Map.FullRange());
        m_Map.State().writeStages = vk::PipelineStageFlagBits2::eClear; // record the clear so the next barrier waits for it
        m_Map.State().writeAccess = vk::AccessFlagBits2::eTransferWrite;
        Vk::TransitionImage(cmd, m_Map, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
                            vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite);

        m_Pipeline.Bind(cmd);
        Vk::Bindings()
            .StorageBuffer(0, dummyShapes.Get())
            .StorageImage(1, m_Map.View())
            .StorageImage(2, m_Atlas.View())
            .StorageBuffer(3, m_State.Get())
            .StorageBuffer(4, m_Alloc.Get())
            .StorageBuffer(5, m_Work.Get())
            .UniformBuffer(6, m_Params[0].Get())
            .Push(cmd, m_Pipeline);
        m_Pipeline.Push(cmd, Push{glm::uvec4(0, 0, 0, kPassInit), glm::uvec4(0)});
        cmd.dispatch(Vk::DivUp(m_Capacity, 512), 1, 1);
    });
    // Waiting on the fence completes the work but does not make its writes visible to
    // later submissions, so the history is kept: the first bake will wait on these writes.
    m_StateSync.writeStages = vk::PipelineStageFlagBits2::eComputeShader;
    m_StateSync.writeAccess = vk::AccessFlagBits2::eShaderStorageWrite;

    MarkAllDirty();
    const double mb = (double(atlasDesc.extent.width) * atlasDesc.extent.height * atlasDesc.extent.depth * 2 +
                       double(settings.cells.x) * settings.cells.y * settings.cells.z * 4) / (1024.0 * 1024.0);
    Log::Info("render", "SDF volume: {}x{}x{} cells of {:.2f} m, {} bricks, {:.0f} MB", settings.cells.x, settings.cells.y,
              settings.cells.z, cell, m_Capacity, mb);
}

void SdfVolume::UpdateParams(uint32_t frameSlot, uint32_t shapeCount) {
    m_ParamsData.gridDim.w = shapeCount;
    m_Params[frameSlot].Write(&m_ParamsData, sizeof(m_ParamsData));
}

void SdfVolume::MarkAllDirty() {
    m_Dirty = true;
    m_DirtyMin = glm::ivec3(0);
    m_DirtyMax = glm::ivec3(m_Settings.cells);
}

void SdfVolume::MarkDirty(const glm::vec3& wMin, const glm::vec3& wMax) {
    // Classification looks this far around each cell centre (see PassClassify), plus a cell of slack.
    const float cell = CellSize();
    const float pad = m_ParamsData.metrics.y + m_Settings.voxelSize * 1.75f + m_ParamsData.metrics.w + cell;
    const glm::ivec3 grid(m_Settings.cells);
    glm::ivec3 lo = glm::clamp(glm::ivec3(glm::floor((wMin - pad - m_Settings.origin) / cell)), glm::ivec3(0), grid);
    glm::ivec3 hi = glm::clamp(glm::ivec3(glm::ceil((wMax + pad - m_Settings.origin) / cell)), glm::ivec3(0), grid);
    if (glm::any(glm::lessThanEqual(hi, lo))) return;
    if (!m_Dirty) {
        m_DirtyMin = lo;
        m_DirtyMax = hi;
        m_Dirty = true;
    } else {
        m_DirtyMin = glm::min(m_DirtyMin, lo);
        m_DirtyMax = glm::max(m_DirtyMax, hi);
    }
}

void SdfVolume::SetShapes(std::span<const Detail::GpuSdfShape> shapes) {
    auto touch = [&](const Detail::GpuSdfShape& s) {
        if (s.operation == static_cast<uint32_t>(SdfOperation::Intersect)) {
            MarkAllDirty(); // intersection clips the whole field
            return;
        }
        float r = s.boundRadius + std::max(s.positionBlend.w, 0.0f);
        glm::vec3 c(s.positionBlend);
        MarkDirty(c - glm::vec3(r), c + glm::vec3(r));
    };
    const size_t n = std::max(shapes.size(), m_Baked.size());
    for (size_t i = 0; i < n; ++i) {
        const Detail::GpuSdfShape* now = i < shapes.size() ? &shapes[i] : nullptr;
        const Detail::GpuSdfShape* was = i < m_Baked.size() ? &m_Baked[i] : nullptr;
        if (now && was && std::memcmp(now, was, sizeof(Detail::GpuSdfShape)) == 0) continue;
        if (now) touch(*now);
        if (was) touch(*was);
    }
    m_Baked.assign(shapes.begin(), shapes.end());
}

void SdfVolume::RecordBake(RenderGraph& graph, vk::CommandBuffer cmd, uint32_t frameSlot, vk::Buffer shapeBuffer) {
    UpdateParams(frameSlot, static_cast<uint32_t>(m_Baked.size()));
    if (!m_Dirty) return;
    m_Dirty = false;

    const glm::uvec3 boxMin(m_DirtyMin);
    const glm::uvec3 boxSize(m_DirtyMax - m_DirtyMin);
    const uint32_t boxCells = boxSize.x * boxSize.y * boxSize.z;
    m_LastBakeCells = boxCells;

    auto bind = [&](vk::CommandBuffer c) {
        m_Pipeline.Bind(c);
        Vk::Bindings()
            .StorageBuffer(0, shapeBuffer)
            .StorageImage(1, m_Map.View())
            .StorageImage(2, m_Atlas.View())
            .StorageBuffer(3, m_State.Get())
            .StorageBuffer(4, m_Alloc.Get())
            .StorageBuffer(5, m_Work.Get())
            .UniformBuffer(6, m_Params[frameSlot].Get())
            .Push(c, m_Pipeline);
    };
    auto push = [&](vk::CommandBuffer c, uint32_t pass) {
        m_Pipeline.Push(c, Push{glm::uvec4(boxMin, pass), glm::uvec4(boxSize, 0)});
    };

    graph.Pass(cmd, "SdfBake.Reset",
        [&](RenderGraph::PassBuilder& b) { b.Buffer(m_WorkSync, Use::TransferDst).Buffer(m_StateSync, Use::TransferDst); },
        [&](vk::CommandBuffer c) {
            // Work header = VkDispatchIndirectCommand {0, 1, 1}; overflow + alloc counters = 0.
            const uint32_t header[4] = {0, 1, 1, 0};
            c.updateBuffer(m_Work.Get(), 0, sizeof(header), header);
            c.fillBuffer(m_State.Get(), 4, 8, 0);
        });

    graph.Pass(cmd, "SdfBake.Classify",
        [&](RenderGraph::PassBuilder& b) {
            b.Image(m_Map, Use::StorageReadWrite)
             .Buffer(m_StateSync, Use::StorageReadWrite)
             .Buffer(m_AllocSync, Use::StorageWrite)
             .Buffer(m_WorkSync, Use::StorageReadWrite);
        },
        [&](vk::CommandBuffer c) {
            bind(c);
            push(c, kPassClassify);
            c.dispatch(Vk::DivUp(boxSize.x, 8), Vk::DivUp(boxSize.y, 8), Vk::DivUp(boxSize.z, 8));
        });

    graph.Pass(cmd, "SdfBake.Allocate",
        [&](RenderGraph::PassBuilder& b) {
            b.Image(m_Map, Use::StorageReadWrite)
             .Buffer(m_StateSync, Use::StorageReadWrite)
             .Buffer(m_AllocSync, Use::StorageRead)
             .Buffer(m_WorkSync, Use::StorageReadWrite);
        },
        [&](vk::CommandBuffer c) {
            bind(c);
            push(c, kPassAllocate);
            c.dispatch(Vk::DivUp(std::min(boxCells, m_Capacity), 512), 1, 1);
        });

    graph.Pass(cmd, "SdfBake.Fill",
        [&](RenderGraph::PassBuilder& b) {
            b.Buffer(m_WorkSync, Use::IndirectRead)
             .Buffer(m_WorkSync, Use::StorageRead)
             .Image(m_Map, Use::StorageReadWrite) // declared writable in SdfBake.comp, so treat it as written
             .Image(m_Atlas, Use::StorageWrite);
        },
        [&](vk::CommandBuffer c) {
            bind(c);
            push(c, kPassFill);
            c.dispatchIndirect(m_Work.Get(), 0);
        });

    graph.Pass(cmd, "SdfBake.Stats",
        [&](RenderGraph::PassBuilder& b) {
            b.Buffer(m_StateSync, Use::TransferSrc).Buffer(m_StatsSync[frameSlot], Use::TransferDst);
        },
        [&](vk::CommandBuffer c) { c.copyBuffer(m_State.Get(), m_Stats[frameSlot].Get(), vk::BufferCopy(0, 0, 16)); });
    graph.Pass(cmd, "SdfBake.StatsToHost",
        [&](RenderGraph::PassBuilder& b) { b.Buffer(m_StatsSync[frameSlot], Use::HostRead); },
        [](vk::CommandBuffer) {});
    m_StatsPending[frameSlot] = true;
}

void SdfVolume::CollectStats(uint32_t frameSlot) {
    if (!m_StatsPending[frameSlot]) return;
    m_StatsPending[frameSlot] = false;
    m_Stats[frameSlot].Invalidate();
    uint32_t header[4];
    std::memcpy(header, m_Stats[frameSlot].Mapped(), sizeof(header));
    m_Used = m_Capacity - std::min(header[0], m_Capacity);
    m_Dropped = header[1];
    m_StatsSync[frameSlot] = {};
}

} // namespace Astral::Render
