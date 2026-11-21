#include "vk_device.hpp"

#include <utility>

namespace astral::gpu {

namespace {
VkCommandBuffer native(void* p) { return static_cast<VkCommandBuffer>(p); }

VkAttachmentLoadOp to_vk(LoadOp op) {
    switch (op) {
    case LoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
    case LoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_CLEAR;
}
VkAttachmentStoreOp to_vk(StoreOp op) {
    return op == StoreOp::Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}
} // namespace

CommandList::CommandList(Device* device, void* native_buffer, std::uint32_t slot) noexcept
    : device_(device), native_(native_buffer), slot_(slot) {}

CommandList::CommandList(CommandList&& other) noexcept
    : device_(std::exchange(other.device_, nullptr)), native_(other.native_), slot_(other.slot_),
      rendering_(other.rendering_), bound_pipeline_(other.bound_pipeline_) {}

CommandList& CommandList::operator=(CommandList&& other) noexcept {
    if (this != &other) {
        this->~CommandList();
        device_ = std::exchange(other.device_, nullptr);
        native_ = other.native_;
        slot_ = other.slot_;
        rendering_ = other.rendering_;
        bound_pipeline_ = other.bound_pipeline_;
    }
    return *this;
}

CommandList::~CommandList() {
    // Dropped without submit: end recording and hand the slot back unused.
    if (!device_) return;
    auto& s = device_->impl();
    if (rendering_) vkCmdEndRendering(native(native_));
    vkEndCommandBuffer(native(native_));
    s.command_slots[slot_].recording = false;
    device_ = nullptr;
}

void CommandList::transition(TextureHandle texture, ResourceState state) {
    auto& s = device_->impl();
    TextureRecord* r = s.textures.get(texture);
    if (!r) {
        s.log_error("transition: invalid texture handle");
        return;
    }
    // Same state is a no-op, except storage writes which still need ordering (write-after-write).
    if (r->state == state && state != ResourceState::ShaderWrite) return;
    const bool depth = is_depth(r->desc.format);
    const StateInfo from = state_info(r->state, depth);
    const StateInfo to = state_info(state, depth);
    VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = from.stages ? from.stages : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    b.srcAccessMask = from.access;
    b.dstStageMask = to.stages;
    b.dstAccessMask = to.access;
    b.oldLayout = from.layout;
    b.newLayout = to.layout;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = r->image;
    b.subresourceRange = {static_cast<VkImageAspectFlags>(depth ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                : VK_IMAGE_ASPECT_COLOR_BIT),
                          0, VK_REMAINING_MIP_LEVELS, 0, 1};
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(native(native_), &dep);
    r->state = state;
}

void CommandList::memory_barrier() {
    VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    b.srcStageMask = b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    b.srcAccessMask = b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &b;
    vkCmdPipelineBarrier2(native(native_), &dep);
}

void CommandList::begin_rendering(const RenderingDesc& desc) {
    auto& s = device_->impl();
    std::vector<VkRenderingAttachmentInfo> colors;
    Extent2D extent = desc.extent;
    for (const auto& c : desc.color) {
        const TextureRecord* r = s.textures.get(c.texture);
        if (!r) {
            s.log_error("begin_rendering: invalid color attachment");
            return;
        }
        if (extent.width == 0) extent = {r->desc.width, r->desc.height};
        VkRenderingAttachmentInfo a{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        a.imageView = r->view;
        a.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        a.loadOp = to_vk(c.load);
        a.storeOp = to_vk(c.store);
        a.clearValue.color = {{c.clear[0], c.clear[1], c.clear[2], c.clear[3]}};
        colors.push_back(a);
    }
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    const TextureRecord* d = s.textures.get(desc.depth.texture);
    if (d) {
        if (extent.width == 0) extent = {d->desc.width, d->desc.height};
        depth.imageView = d->view;
        depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth.loadOp = to_vk(desc.depth.load);
        depth.storeOp = to_vk(desc.depth.store);
        depth.clearValue.depthStencil = {desc.depth.clear, 0};
    }
    VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};
    info.renderArea = {{0, 0}, {extent.width, extent.height}};
    info.layerCount = 1;
    info.colorAttachmentCount = static_cast<std::uint32_t>(colors.size());
    info.pColorAttachments = colors.data();
    info.pDepthAttachment = d ? &depth : nullptr;
    vkCmdBeginRendering(native(native_), &info);
    rendering_ = true;
    set_viewport(0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height));
    set_scissor(0, 0, extent.width, extent.height);
}

void CommandList::end_rendering() {
    if (!rendering_) return;
    vkCmdEndRendering(native(native_));
    rendering_ = false;
}

void CommandList::set_viewport(float x, float y, float width, float height) {
    const VkViewport v{x, y, width, height, 0.0f, 1.0f};
    vkCmdSetViewport(native(native_), 0, 1, &v);
}

void CommandList::set_scissor(std::int32_t x, std::int32_t y, std::uint32_t width, std::uint32_t height) {
    const VkRect2D r{{x, y}, {width, height}};
    vkCmdSetScissor(native(native_), 0, 1, &r);
}

void CommandList::bind_pipeline(PipelineHandle pipeline) {
    auto& s = device_->impl();
    const PipelineRecord* r = s.pipelines.get(pipeline);
    if (!r) {
        s.log_error("bind_pipeline: invalid pipeline handle");
        return;
    }
    vkCmdBindPipeline(native(native_), r->bind_point, r->pipeline);
    bound_pipeline_ = pipeline;
}

void CommandList::push_constants(const void* data, std::uint32_t size, std::uint32_t offset) {
    if (size + offset > max_push_constant_size) {
        device_->impl().log_error("push_constants: exceeds 128 bytes");
        return;
    }
    vkCmdPushConstants(native(native_), device_->impl().pipeline_layout, VK_SHADER_STAGE_ALL,
                       offset, size, data);
}

void CommandList::bind_index_buffer(BufferHandle buffer, IndexType type, std::uint64_t offset) {
    const BufferRecord* r = device_->impl().buffers.get(buffer);
    if (!r) return;
    vkCmdBindIndexBuffer(native(native_), r->buffer, offset,
                         type == IndexType::Uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void CommandList::draw(std::uint32_t vertex_count, std::uint32_t instance_count,
                       std::uint32_t first_vertex, std::uint32_t first_instance) {
    vkCmdDraw(native(native_), vertex_count, instance_count, first_vertex, first_instance);
}

void CommandList::draw_indexed(std::uint32_t index_count, std::uint32_t instance_count,
                               std::uint32_t first_index, std::int32_t vertex_offset,
                               std::uint32_t first_instance) {
    vkCmdDrawIndexed(native(native_), index_count, instance_count, first_index, vertex_offset,
                     first_instance);
}

void CommandList::dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    vkCmdDispatch(native(native_), x, y, z);
}

void CommandList::copy_buffer(BufferHandle src, BufferHandle dst, std::uint64_t size,
                              std::uint64_t src_offset, std::uint64_t dst_offset) {
    auto& s = device_->impl();
    const BufferRecord* a = s.buffers.get(src);
    const BufferRecord* b = s.buffers.get(dst);
    if (!a || !b) return;
    const VkBufferCopy region{src_offset, dst_offset, size};
    vkCmdCopyBuffer(native(native_), a->buffer, b->buffer, 1, &region);
}

void CommandList::copy_texture_to_buffer(TextureHandle src, BufferHandle dst) {
    auto& s = device_->impl();
    const TextureRecord* t = s.textures.get(src);
    const BufferRecord* b = s.buffers.get(dst);
    if (!t || !b) return;
    VkBufferImageCopy region{};
    region.imageSubresource = {static_cast<VkImageAspectFlags>(is_depth(t->desc.format)
                                                                   ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                   : VK_IMAGE_ASPECT_COLOR_BIT),
                               0, 0, 1};
    region.imageExtent = {t->desc.width, t->desc.height, 1};
    vkCmdCopyImageToBuffer(native(native_), t->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, b->buffer, 1, &region);
}

void CommandList::copy_buffer_to_texture(BufferHandle src, TextureHandle dst) {
    auto& s = device_->impl();
    const BufferRecord* b = s.buffers.get(src);
    const TextureRecord* t = s.textures.get(dst);
    if (!t || !b) return;
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {t->desc.width, t->desc.height, 1};
    vkCmdCopyBufferToImage(native(native_), b->buffer, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void CommandList::begin_debug_label(const char* name) {
    auto& s = device_->impl();
    if (!s.cmd_begin_label) return;
    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    s.cmd_begin_label(native(native_), &label);
}

void CommandList::end_debug_label() {
    auto& s = device_->impl();
    if (s.cmd_end_label) s.cmd_end_label(native(native_));
}

} // namespace astral::gpu
