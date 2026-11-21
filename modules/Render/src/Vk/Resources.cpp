#include "Vk/Resources.hpp"
#include "Vk/Device.hpp"

#include <cstring>
#include <utility>

namespace Astral::Render::Vk {

// ----------------------------------------------------------------------------- Buffer

Buffer::Buffer(Device& device, vk::DeviceSize size, vk::BufferUsageFlags usage, MemoryUsage memory, const std::string& name)
    : m_Device(&device), m_Size(size) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO;
    if (memory == MemoryUsage::Upload)
        alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    else if (memory == MemoryUsage::Readback)
        alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer buffer;
    VmaAllocationInfo allocInfo{};
    Check(vmaCreateBuffer(device.Allocator(), &info, &alloc, &buffer, &m_Allocation, &allocInfo), "vmaCreateBuffer");
    m_Buffer = buffer;
    m_Mapped = allocInfo.pMappedData;
    device.SetDebugName(vk::ObjectType::eBuffer, reinterpret_cast<uint64_t>(buffer), name);
}

Buffer::~Buffer() { Release(); }

Buffer::Buffer(Buffer&& o) noexcept { *this = std::move(o); }

Buffer& Buffer::operator=(Buffer&& o) noexcept {
    if (this != &o) {
        Release();
        m_Device = std::exchange(o.m_Device, nullptr);
        m_Buffer = std::exchange(o.m_Buffer, nullptr);
        m_Allocation = std::exchange(o.m_Allocation, VK_NULL_HANDLE);
        m_Size = std::exchange(o.m_Size, 0);
        m_Mapped = std::exchange(o.m_Mapped, nullptr);
    }
    return *this;
}

void Buffer::Release() {
    if (!m_Buffer) return;
    VmaAllocator allocator = m_Device->Allocator();
    VkBuffer buffer = m_Buffer;
    VmaAllocation allocation = m_Allocation;
    m_Device->Retire([=] { vmaDestroyBuffer(allocator, buffer, allocation); });
    m_Buffer = nullptr;
    m_Allocation = VK_NULL_HANDLE;
    m_Mapped = nullptr;
}

void Buffer::Write(const void* data, vk::DeviceSize size, vk::DeviceSize offset) {
    std::memcpy(static_cast<uint8_t*>(m_Mapped) + offset, data, size);
    vmaFlushAllocation(m_Device->Allocator(), m_Allocation, offset, size);
}

void Buffer::Invalidate() { vmaInvalidateAllocation(m_Device->Allocator(), m_Allocation, 0, VK_WHOLE_SIZE); }

// ----------------------------------------------------------------------------- Image

bool IsDepthFormat(vk::Format f) {
    return f == vk::Format::eD32Sfloat || f == vk::Format::eD16Unorm || f == vk::Format::eD24UnormS8Uint ||
           f == vk::Format::eD32SfloatS8Uint;
}

Image::Image(Device& device, const ImageDesc& desc) : m_Device(&device), m_Desc(desc) {
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = desc.is3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    info.format = static_cast<VkFormat>(desc.format);
    info.extent = desc.extent;
    info.mipLevels = desc.mipLevels;
    info.arrayLayers = desc.layers;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = static_cast<VkImageUsageFlags>(desc.usage);
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (desc.cube) info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VkImage image;
    Check(vmaCreateImage(device.Allocator(), &info, &alloc, &image, &m_Allocation, nullptr), "vmaCreateImage");
    m_Image = image;
    device.SetDebugName(vk::ObjectType::eImage, reinterpret_cast<uint64_t>(image), desc.name);

    vk::ImageViewType viewType = desc.is3D ? vk::ImageViewType::e3D
                               : desc.cube ? vk::ImageViewType::eCube
                               : desc.layers > 1 ? vk::ImageViewType::e2DArray
                                                 : vk::ImageViewType::e2D;
    vk::ImageViewCreateInfo view({}, m_Image, viewType, desc.format, {}, FullRange());
    m_View = device.Get().createImageView(view);

    if (desc.cube) { // storage writes into cubes go through a 2D array view
        view.viewType = vk::ImageViewType::e2DArray;
        m_ArrayView = device.Get().createImageView(view);
    }
    if (desc.mipLevels > 1) {
        for (uint32_t mip = 0; mip < desc.mipLevels; ++mip) {
            vk::ImageViewCreateInfo mipView = view;
            mipView.viewType = desc.cube || desc.layers > 1 ? vk::ImageViewType::e2DArray : viewType;
            mipView.subresourceRange.baseMipLevel = mip;
            mipView.subresourceRange.levelCount = 1;
            m_MipViews.push_back(device.Get().createImageView(mipView));
        }
    }
}

Image::~Image() { Release(); }

Image::Image(Image&& o) noexcept { *this = std::move(o); }

Image& Image::operator=(Image&& o) noexcept {
    if (this != &o) {
        Release();
        m_Device = std::exchange(o.m_Device, nullptr);
        m_Desc = std::move(o.m_Desc);
        m_Image = std::exchange(o.m_Image, nullptr);
        m_Allocation = std::exchange(o.m_Allocation, VK_NULL_HANDLE);
        m_View = std::exchange(o.m_View, nullptr);
        m_ArrayView = std::exchange(o.m_ArrayView, nullptr);
        m_MipViews = std::move(o.m_MipViews);
        m_State = std::exchange(o.m_State, {});
    }
    return *this;
}

void Image::Release() {
    if (!m_Image) return;
    vk::Device device = m_Device->Get();
    VmaAllocator allocator = m_Device->Allocator();
    VkImage image = m_Image;
    VmaAllocation allocation = m_Allocation;
    std::vector<vk::ImageView> views = std::move(m_MipViews);
    views.push_back(m_View);
    if (m_ArrayView) views.push_back(m_ArrayView);
    m_Device->Retire([=] {
        for (vk::ImageView v : views) device.destroyImageView(v);
        vmaDestroyImage(allocator, image, allocation);
    });
    m_Image = nullptr;
    m_View = nullptr;
    m_ArrayView = nullptr;
}

vk::ImageView Image::MipView(uint32_t mip) const { return m_MipViews.empty() ? LayerArrayView() : m_MipViews[mip]; }

vk::ImageAspectFlags Image::Aspect() const {
    return IsDepthFormat(m_Desc.format) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
}

vk::ImageSubresourceRange Image::FullRange() const {
    return {Aspect(), 0, m_Desc.mipLevels, 0, m_Desc.layers};
}

void TransitionImage(vk::CommandBuffer cmd, Image& image, vk::ImageLayout layout,
                     vk::PipelineStageFlags2 stages, vk::AccessFlags2 access) {
    ImageState& s = image.State();
    vk::ImageMemoryBarrier2 barrier;
    barrier.srcStageMask = (s.writeStages | s.readStages) ? (s.writeStages | s.readStages)
                                                          : vk::PipelineStageFlags2(vk::PipelineStageFlagBits2::eTopOfPipe);
    barrier.srcAccessMask = s.writeAccess;
    barrier.dstStageMask = stages;
    barrier.dstAccessMask = access;
    barrier.oldLayout = s.layout;
    barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.Get();
    barrier.subresourceRange = image.FullRange();
    vk::DependencyInfo dep;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    cmd.pipelineBarrier2(dep);
    // A layout transition counts as a write: later uses must wait for it.
    s = {};
    s.layout = layout;
    s.writeStages = stages;
    s.visibleStages = stages;
    s.visibleAccess = access;
}

void UploadImage(Device& device, Image& image, const void* pixels, size_t size, bool generateMips) {
    Buffer staging(device, size, vk::BufferUsageFlagBits::eTransferSrc, MemoryUsage::Upload, "texture staging");
    staging.Write(pixels, size);
    const auto extent = image.Extent();
    const uint32_t mips = image.Desc().mipLevels;

    device.ImmediateSubmit([&](vk::CommandBuffer cmd) {
        TransitionImage(cmd, image, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer,
                        vk::AccessFlagBits2::eTransferWrite);
        vk::BufferImageCopy copy;
        copy.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        copy.imageExtent = extent;
        cmd.copyBufferToImage(staging.Get(), image.Get(), vk::ImageLayout::eTransferDstOptimal, copy);

        if (generateMips && mips > 1) {
            int32_t w = static_cast<int32_t>(extent.width), h = static_cast<int32_t>(extent.height);
            for (uint32_t mip = 1; mip < mips; ++mip) {
                vk::ImageMemoryBarrier2 toSrc;
                toSrc.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
                toSrc.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
                toSrc.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
                toSrc.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
                toSrc.oldLayout = vk::ImageLayout::eTransferDstOptimal;
                toSrc.newLayout = vk::ImageLayout::eTransferSrcOptimal;
                toSrc.image = image.Get();
                toSrc.subresourceRange = {vk::ImageAspectFlagBits::eColor, mip - 1, 1, 0, 1};
                cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, toSrc));

                vk::ImageBlit blit;
                blit.srcSubresource = {vk::ImageAspectFlagBits::eColor, mip - 1, 0, 1};
                blit.srcOffsets[1] = vk::Offset3D{w, h, 1};
                w = std::max(1, w / 2);
                h = std::max(1, h / 2);
                blit.dstSubresource = {vk::ImageAspectFlagBits::eColor, mip, 0, 1};
                blit.dstOffsets[1] = vk::Offset3D{w, h, 1};
                cmd.blitImage(image.Get(), vk::ImageLayout::eTransferSrcOptimal, image.Get(),
                              vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);
            }
            // Bring every mip to TransferDst so one transition covers the whole image.
            vk::ImageMemoryBarrier2 back;
            back.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            back.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
            back.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
            back.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
            back.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
            back.newLayout = vk::ImageLayout::eTransferDstOptimal;
            back.image = image.Get();
            back.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mips - 1, 0, 1};
            cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, back));
        }
        TransitionImage(cmd, image, vk::ImageLayout::eShaderReadOnlyOptimal,
                        vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader,
                        vk::AccessFlagBits2::eShaderSampledRead);
    });
}

} // namespace Astral::Render::Vk
