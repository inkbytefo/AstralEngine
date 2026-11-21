#include "Graph/RenderGraph.hpp"

namespace Astral::Render {

namespace {

using Stage = vk::PipelineStageFlagBits2;
using Access = vk::AccessFlagBits2;

struct Target {
    vk::ImageLayout layout;
    vk::PipelineStageFlags2 stages;
    vk::AccessFlags2 access;
    bool writes;
};

constexpr vk::PipelineStageFlags2 kShaderStages = Stage::eVertexShader | Stage::eFragmentShader | Stage::eComputeShader;

Target Resolve(Use use, bool depth) {
    switch (use) {
        case Use::ColorAttachment:
            return {vk::ImageLayout::eColorAttachmentOptimal, Stage::eColorAttachmentOutput,
                    Access::eColorAttachmentWrite | Access::eColorAttachmentRead, true};
        case Use::DepthAttachment:
            return {vk::ImageLayout::eDepthAttachmentOptimal, Stage::eEarlyFragmentTests | Stage::eLateFragmentTests,
                    Access::eDepthStencilAttachmentWrite | Access::eDepthStencilAttachmentRead, true};
        case Use::Sampled:
            return {depth ? vk::ImageLayout::eDepthReadOnlyOptimal : vk::ImageLayout::eShaderReadOnlyOptimal,
                    kShaderStages, Access::eShaderSampledRead, false};
        case Use::StorageRead:
            return {vk::ImageLayout::eGeneral, kShaderStages, Access::eShaderStorageRead, false};
        case Use::StorageWrite:
            return {vk::ImageLayout::eGeneral, kShaderStages, Access::eShaderStorageWrite, true};
        case Use::StorageReadWrite:
            return {vk::ImageLayout::eGeneral, kShaderStages, Access::eShaderStorageRead | Access::eShaderStorageWrite, true};
        case Use::VertexIndexRead:
            return {vk::ImageLayout::eUndefined, Stage::eVertexInput, Access::eVertexAttributeRead | Access::eIndexRead, false};
        case Use::IndirectRead:
            return {vk::ImageLayout::eUndefined, Stage::eDrawIndirect, Access::eIndirectCommandRead, false};
        case Use::TransferSrc:
            return {vk::ImageLayout::eTransferSrcOptimal, Stage::eAllTransfer, Access::eTransferRead, false};
        case Use::TransferDst:
            return {vk::ImageLayout::eTransferDstOptimal, Stage::eAllTransfer, Access::eTransferWrite, true};
        case Use::HostRead:
            return {vk::ImageLayout::eUndefined, Stage::eHost, Access::eHostRead, false};
    }
    return {};
}

} // namespace

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Image(Vk::Image& image, Use use, bool discard) {
    m_Images.push_back({image.Get(), &image.State(), image.FullRange(), use, discard, Vk::IsDepthFormat(image.Format())});
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::RawImage(vk::Image image, Vk::ImageState& state,
                                                              vk::ImageSubresourceRange range, Use use, bool discard) {
    m_Images.push_back({image, &state, range, use, discard, range.aspectMask == vk::ImageAspectFlagBits::eDepth});
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Buffer(BufferState& state, Use use) {
    m_Buffers.push_back({&state, use});
    return *this;
}

namespace {

struct Requirement {
    vk::ImageLayout layout;
    vk::PipelineStageFlags2 stages;
    vk::AccessFlags2 access;
    bool writes;
};

// Decides whether a barrier is needed for `req` given the resource's history, fills the
// barrier's stage/access masks if so, and updates the history.
bool Synchronise(Vk::SyncState& s, bool layoutChange, const Requirement& req,
                 vk::PipelineStageFlags2& srcStages, vk::AccessFlags2& srcAccess) {
    if (!req.writes && !layoutChange) {
        bool alreadyVisible = !s.writeAccess ||
                              ((req.stages & s.visibleStages) == req.stages && (req.access & s.visibleAccess) == req.access);
        if (alreadyVisible && (!s.writeStages || (req.stages & s.visibleStages) == req.stages)) {
            s.readStages |= req.stages;
            return false;
        }
        // Read-after-write: make the last write visible to this reader.
        srcStages = s.writeStages ? s.writeStages : vk::PipelineStageFlags2(Stage::eTopOfPipe);
        srcAccess = s.writeAccess;
        s.visibleStages |= req.stages;
        s.visibleAccess |= req.access;
        s.readStages |= req.stages;
        return true;
    }
    // Write (or layout transition): wait for the last write and every reader since.
    vk::PipelineStageFlags2 waitStages = s.writeStages | s.readStages;
    srcStages = waitStages ? waitStages : vk::PipelineStageFlags2(Stage::eTopOfPipe);
    srcAccess = s.writeAccess;
    s.writeStages = req.stages;
    s.writeAccess = req.access & ~(Access::eShaderStorageRead | Access::eShaderSampledRead | Access::eColorAttachmentRead |
                                   Access::eDepthStencilAttachmentRead | Access::eTransferRead | Access::eIndirectCommandRead |
                                   Access::eVertexAttributeRead | Access::eIndexRead | Access::eHostRead);
    // A write is not yet visible to anyone. A read-only layout transition, however, has
    // just made earlier writes visible to its reader, which also counts as a reader.
    s.visibleStages = req.writes ? vk::PipelineStageFlags2{} : req.stages;
    s.visibleAccess = req.writes ? vk::AccessFlags2{} : req.access;
    s.readStages = req.writes ? vk::PipelineStageFlags2{} : req.stages;
    return true;
}

} // namespace

void RenderGraph::Pass(vk::CommandBuffer cmd, const char* name, const std::function<void(PassBuilder&)>& declare,
                       const std::function<void(vk::CommandBuffer)>& execute) {
    PassBuilder builder;
    declare(builder);

    std::vector<vk::ImageMemoryBarrier2> imageBarriers;
    std::vector<vk::MemoryBarrier2> memoryBarriers;

    // Merge repeated uses of the same image.
    std::vector<std::pair<PassBuilder::ImageUse, Requirement>> images;
    for (auto& u : builder.m_Images) {
        Target t = Resolve(u.use, u.depth);
        Requirement r{t.layout, t.stages, t.access, t.writes};
        bool merged = false;
        for (auto& [prev, req] : images) {
            if (prev.state == u.state) {
                req.stages |= r.stages;
                req.access |= r.access;
                req.writes |= r.writes;
                prev.discard &= u.discard;
                merged = true;
            }
        }
        if (!merged) images.push_back({u, r});
    }

    for (auto& [u, req] : images) {
        Vk::ImageState& s = *u.state;
        bool layoutChange = s.layout != req.layout || u.discard;
        vk::ImageLayout oldLayout = u.discard ? vk::ImageLayout::eUndefined : s.layout;
        vk::PipelineStageFlags2 srcStages;
        vk::AccessFlags2 srcAccess;
        if (!Synchronise(s, layoutChange, req, srcStages, srcAccess)) continue;
        s.layout = req.layout;
        vk::ImageMemoryBarrier2 b;
        b.srcStageMask = srcStages;
        b.srcAccessMask = srcAccess;
        b.dstStageMask = req.stages;
        b.dstAccessMask = req.access;
        b.oldLayout = oldLayout;
        b.newLayout = req.layout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = u.image;
        b.subresourceRange = u.range;
        imageBarriers.push_back(b);
    }

    std::vector<std::pair<BufferState*, Requirement>> buffers;
    for (auto& u : builder.m_Buffers) {
        Target t = Resolve(u.use, false);
        Requirement r{vk::ImageLayout::eUndefined, t.stages, t.access, t.writes};
        bool merged = false;
        for (auto& [state, req] : buffers) {
            if (state == u.state) {
                req.stages |= r.stages;
                req.access |= r.access;
                req.writes |= r.writes;
                merged = true;
            }
        }
        if (!merged) buffers.push_back({u.state, r});
    }

    for (auto& [state, req] : buffers) {
        vk::PipelineStageFlags2 srcStages;
        vk::AccessFlags2 srcAccess;
        if (!Synchronise(*state, false, req, srcStages, srcAccess)) continue;
        vk::MemoryBarrier2 b;
        b.srcStageMask = srcStages;
        b.srcAccessMask = srcAccess;
        b.dstStageMask = req.stages;
        b.dstAccessMask = req.access;
        memoryBarriers.push_back(b);
    }

    if (m_DebugLabels) cmd.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT(name));
    if (!imageBarriers.empty() || !memoryBarriers.empty()) {
        vk::DependencyInfo dep;
        dep.memoryBarrierCount = static_cast<uint32_t>(memoryBarriers.size());
        dep.pMemoryBarriers = memoryBarriers.data();
        dep.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
        dep.pImageMemoryBarriers = imageBarriers.data();
        cmd.pipelineBarrier2(dep);
    }
    execute(cmd);
    if (m_DebugLabels) cmd.endDebugUtilsLabelEXT();
}

} // namespace Astral::Render
