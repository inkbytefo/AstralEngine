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
        return {VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE};
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
    return BufferHandle{s.buffers.create(record).index, 0} == BufferHandle{}
               ? BufferHandle{}
               : BufferHandle{};
}

} // namespace astral::gpu
