#include "vk_device.hpp"

#include <cstring>

namespace astral::gpu {

// --- Format and state tables -------------------------------------------------------------

VkFormat to_vk(Format format) noexcept {
    switch (format) {
    case Format::Undefined: return VK_FORMAT_UNDEFINED;
    case Format::RGBA8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::RGBA8Srgb: return VK_FORMAT_R8G8B8A8_SRGB;
    case Format::BGRA8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
    case Format::BGRA8Srgb: return VK_FORMAT_B8G8R8A8_SRGB;
    case Format::RG16Float: return VK_FORMAT_R16G16_SFLOAT;
    case Format::RGBA16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Format::R32Float: return VK_FORMAT_R32_SFLOAT;
    case Format::RGBA32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Format::R11G11B10Float: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case Format::D32Float: return VK_FORMAT_D32_SFLOAT;
    }
    return VK_FORMAT_UNDEFINED;
}

Format from_vk(VkFormat format) noexcept {
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM: return Format::RGBA8Unorm;
    case VK_FORMAT_R8G8B8A8_SRGB: return Format::RGBA8Srgb;
    case VK_FORMAT_B8G8R8A8_UNORM: return Format::BGRA8Unorm;
    case VK_FORMAT_B8G8R8A8_SRGB: return Format::BGRA8Srgb;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return Format::RGBA16Float;
    default: return Format::Undefined;
    }
}

std::uint32_t format_size(Format format) noexcept {
    switch (format) {
    case Format::Undefined: return 0;
    case Format::RGBA8Unorm:
    case Format::RGBA8Srgb:
    case Format::BGRA8Unorm:
    case Format::BGRA8Srgb:
    case Format::RG16Float:
    case Format::R32Float:
    case Format::R11G11B10Float:
    case Format::D32Float: return 4;
    case Format::RGBA16Float: return 8;
    case Format::RGBA32Float: return 16;
    }
    return 0;
}

bool is_depth(Format format) noexcept { return format == Format::D32Float; }

StateInfo state_info(ResourceState state, bool depth) noexcept {
    switch (state) {
    case ResourceState::Undefined:
        // As a source stage this must chain with a swapchain acquire wait (which waits at
        // color output / transfer), otherwise the layout change could run before the
        // presentation engine released the image.
        return {VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                VK_ACCESS_2_NONE};
    case ResourceState::ColorTarget:
        return {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
    case ResourceState::DepthTarget:
        return {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
    case ResourceState::ShaderRead:
        return {depth ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};
    case ResourceState::ShaderWrite:
        return {VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    case ResourceState::TransferSrc:
        return {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT};
    case ResourceState::TransferDst:
        return {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT};
    case ResourceState::Present:
        return {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_NONE};
    }
    return {VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE};
}

// --- Buffers ---------------------------------------------------------------------------

namespace {

VkBufferUsageFlags buffer_usage(BufferUsage usage) {
    // Every buffer can be copied and addressed: cheap, and keeps staging/readback simple.
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::Index)) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::Indirect)) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    return flags;
}

bool create_raw_buffer(Device::Impl& s, std::uint64_t size, BufferUsage usage, MemoryUsage memory,
                       BufferRecord& out) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = buffer_usage(usage);
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO;
    if (memory == MemoryUsage::CpuToGpu) {
        alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;
    } else if (memory == MemoryUsage::GpuToCpu) {
        alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    } else {
        alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }

    VmaAllocationInfo result{};
    if (vmaCreateBuffer(s.allocator, &info, &alloc, &out.buffer, &out.allocation, &result) != VK_SUCCESS) {
        return false;
    }
    out.mapped = result.pMappedData;
    out.size = size;
    out.usage = usage;
    out.memory = memory;
    VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    address.buffer = out.buffer;
    out.address = vkGetBufferDeviceAddress(s.device, &address);
    return true;
}

bool in_bounds(std::uint64_t offset, std::uint64_t size, std::uint64_t total) {
    return size <= total && offset <= total - size;
}

} // namespace

void destroy_buffer_record(Device::Impl& s, const BufferRecord& r) {
    vmaDestroyBuffer(s.allocator, r.buffer, r.allocation);
}

void destroy_texture_record(Device::Impl& s, const TextureRecord& r) {
    vkDestroyImageView(s.device, r.view, nullptr);
    if (r.allocation) vmaDestroyImage(s.allocator, r.image, r.allocation);
}

BufferHandle Device::create_buffer(const BufferDesc& desc) {
    auto& s = *impl_;
    if (desc.size == 0) {
        s.log_error("create_buffer: size must be > 0");
        return {};
    }
    BufferRecord record;
    if (!create_raw_buffer(s, desc.size, desc.usage, desc.memory, record)) {
        s.log_error("create_buffer: allocation failed");
        return {};
    }
    s.name_object(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(record.buffer), desc.debug_name);
    return s.buffers.create(record);
}

void Device::destroy(BufferHandle buffer) {
    auto& s = *impl_;
    const BufferRecord* r = s.buffers.get(buffer);
    if (!r) return;
    const BufferRecord copy = *r;
    s.buffers.destroy(buffer); // handle is dead immediately; memory lives until the GPU is done
    s.retire([&s, copy] { destroy_buffer_record(s, copy); });
}

std::uint64_t Device::buffer_address(BufferHandle buffer) const noexcept {
    const BufferRecord* r = impl_->buffers.get(buffer);
    return r ? r->address : 0;
}

std::uint64_t Device::buffer_size(BufferHandle buffer) const noexcept {
    const BufferRecord* r = impl_->buffers.get(buffer);
    return r ? r->size : 0;
}

bool Device::write_buffer(BufferHandle buffer, const void* data, std::uint64_t size,
                          std::uint64_t offset) {
    auto& s = *impl_;
    const BufferRecord* r = s.buffers.get(buffer);
    if (!r || !data || !in_bounds(offset, size, r->size)) return false;
    if (r->mapped) {
        std::memcpy(static_cast<std::byte*>(r->mapped) + offset, data, size);
        vmaFlushAllocation(s.allocator, r->allocation, offset, size);
        return true;
    }
    if (!s.can_run_immediate("write_buffer")) return false;
    BufferRecord staging;
    if (!create_raw_buffer(s, size, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu, staging)) return false;
    std::memcpy(staging.mapped, data, size);
    vmaFlushAllocation(s.allocator, staging.allocation, 0, size);
    const VkBuffer dst = r->buffer;
    s.immediate([&](VkCommandBuffer cmd) {
        VkBufferCopy region{0, offset, size};
        vkCmdCopyBuffer(cmd, staging.buffer, dst, 1, &region);
    });
    destroy_buffer_record(s, staging);
    return true;
}

bool Device::read_buffer(BufferHandle buffer, void* out, std::uint64_t size, std::uint64_t offset) {
    auto& s = *impl_;
    const BufferRecord* r = s.buffers.get(buffer);
    if (!r || !out || !in_bounds(offset, size, r->size)) return false;
    s.wait_value(s.last_submitted); // results of pending work must be visible
    if (r->mapped && r->memory == MemoryUsage::GpuToCpu) {
        vmaInvalidateAllocation(s.allocator, r->allocation, offset, size);
        std::memcpy(out, static_cast<const std::byte*>(r->mapped) + offset, size);
        return true;
    }
    if (!s.can_run_immediate("read_buffer")) return false;
    BufferRecord staging;
    if (!create_raw_buffer(s, size, BufferUsage::TransferDst, MemoryUsage::GpuToCpu, staging)) return false;
    const VkBuffer src = r->buffer;
    s.immediate([&](VkCommandBuffer cmd) {
        VkBufferCopy region{offset, 0, size};
        vkCmdCopyBuffer(cmd, src, staging.buffer, 1, &region);
    });
    vmaInvalidateAllocation(s.allocator, staging.allocation, 0, size);
    std::memcpy(out, staging.mapped, size);
    destroy_buffer_record(s, staging);
    return true;
}

// --- Textures --------------------------------------------------------------------------

namespace {

VkImageUsageFlags image_usage(TextureUsage usage) {
    VkImageUsageFlags flags = 0;
    if (has_flag(usage, TextureUsage::Sampled)) flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (has_flag(usage, TextureUsage::Storage)) flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (has_flag(usage, TextureUsage::ColorTarget)) flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (has_flag(usage, TextureUsage::DepthTarget)) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (has_flag(usage, TextureUsage::TransferSrc)) flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (has_flag(usage, TextureUsage::TransferDst)) flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return flags;
}

} // namespace

VkImageView create_view(Device::Impl& s, VkImage image, VkFormat format, bool depth,
                        std::uint32_t mips) {
    VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.subresourceRange = {static_cast<VkImageAspectFlags>(depth ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                   : VK_IMAGE_ASPECT_COLOR_BIT),
                             0, mips, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    vkCreateImageView(s.device, &info, nullptr, &view);
    return view;
}

TextureHandle Device::create_texture(const TextureDesc& desc) {
    auto& s = *impl_;
    if (desc.width == 0 || desc.height == 0 || desc.format == Format::Undefined ||
        desc.usage == TextureUsage::None || desc.mip_levels == 0) {
        s.log_error("create_texture: invalid description");
        return {};
    }
    TextureRecord record;
    record.desc = desc;
    record.desc.debug_name = {};

    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = to_vk(desc.format);
    info.extent = {desc.width, desc.height, 1};
    info.mipLevels = desc.mip_levels;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = image_usage(desc.usage);
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if (vmaCreateImage(s.allocator, &info, &alloc, &record.image, &record.allocation, nullptr) != VK_SUCCESS) {
        s.log_error("create_texture: allocation failed");
        return {};
    }
    record.view = create_view(s, record.image, info.format, is_depth(desc.format), desc.mip_levels);
    if (has_flag(desc.usage, TextureUsage::Sampled) || has_flag(desc.usage, TextureUsage::Storage)) {
        record.bindless = s.allocate_bindless();
        if (record.bindless == UINT32_MAX) {
            s.log_error("create_texture: bindless table is full");
            destroy_texture_record(s, record);
            return {};
        }
        s.write_texture_descriptors(record);
    }
    s.name_object(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(record.image), desc.debug_name);
    return s.textures.create(record);
}

void Device::destroy(TextureHandle texture) {
    auto& s = *impl_;
    const TextureRecord* r = s.textures.get(texture);
    if (!r) return;
    const TextureRecord copy = *r;
    s.textures.destroy(texture);
    s.retire([&s, copy] {
        destroy_texture_record(s, copy);
        s.free_bindless_index(copy.bindless); // reusable only once the GPU stopped reading it
    });
}

std::uint32_t Device::bindless_index(TextureHandle texture) const noexcept {
    const TextureRecord* r = impl_->textures.get(texture);
    return r ? r->bindless : UINT32_MAX;
}

TextureDesc Device::texture_desc(TextureHandle texture) const noexcept {
    const TextureRecord* r = impl_->textures.get(texture);
    return r ? r->desc : TextureDesc{};
}

bool Device::write_texture(TextureHandle texture, const void* data, std::uint64_t size) {
    auto& s = *impl_;
    TextureRecord* r = s.textures.get(texture);
    if (!r || !data) return false;
    const std::uint64_t expected =
        std::uint64_t{r->desc.width} * r->desc.height * format_size(r->desc.format);
    if (size != expected) return false;
    if (!s.can_run_immediate("write_texture")) return false;

    BufferRecord staging;
    if (!create_raw_buffer(s, size, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu, staging)) return false;
    std::memcpy(staging.mapped, data, size);
    vmaFlushAllocation(s.allocator, staging.allocation, 0, size);

    const bool depth = is_depth(r->desc.format);
    const StateInfo from = state_info(r->state, depth);
    const StateInfo dst = state_info(ResourceState::TransferDst, depth);
    const StateInfo read = state_info(ResourceState::ShaderRead, depth);
    const VkImage image = r->image;
    const VkExtent3D extent{r->desc.width, r->desc.height, 1};
    const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    s.immediate([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.srcStageMask = from.stages;
        b.srcAccessMask = from.access;
        b.dstStageMask = dst.stages;
        b.dstAccessMask = dst.access;
        b.oldLayout = from.layout;
        b.newLayout = dst.layout;
        b.image = image;
        b.subresourceRange = {aspect, 0, VK_REMAINING_MIP_LEVELS, 0, 1};
        VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &b;
        vkCmdPipelineBarrier2(cmd, &dep);

        VkBufferImageCopy region{};
        region.imageSubresource = {aspect, 0, 0, 1};
        region.imageExtent = extent;
        vkCmdCopyBufferToImage(cmd, staging.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        b.srcStageMask = dst.stages;
        b.srcAccessMask = dst.access;
        b.dstStageMask = read.stages;
        b.dstAccessMask = read.access;
        b.oldLayout = dst.layout;
        b.newLayout = read.layout;
        vkCmdPipelineBarrier2(cmd, &dep);
    });
    r->state = ResourceState::ShaderRead;
    destroy_buffer_record(s, staging);
    return true;
}

} // namespace astral::gpu
