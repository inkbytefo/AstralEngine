#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/RenderTargets.hpp"
#include "Astral/Renderer/VulkanContext.hpp"

#include <stdexcept>
#include <string>

namespace Astral {

// =============================================================================
// OwnedImage Implementasyonu
// =============================================================================

OwnedImage::OwnedImage(VmaAllocator allocator,
                       vk::Device device,
                       const vk::ImageCreateInfo& imageInfo,
                       const VmaAllocationCreateInfo& allocInfo,
                       vk::ImageAspectFlags aspectFlags)
    : m_Allocator(allocator),
      m_Device(device),
      m_Format(imageInfo.format),
      m_Extent(vk::Extent2D{ imageInfo.extent.width, imageInfo.extent.height }) {
    if (allocator == VK_NULL_HANDLE || !device) {
        throw std::invalid_argument("[Astral::OwnedImage] Gecersiz allocator veya device!");
    }

    VkImage rawImg = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkImageCreateInfo rawImageInfo = static_cast<VkImageCreateInfo>(imageInfo);
    VkResult res = vmaCreateImage(allocator, &rawImageInfo, &allocInfo, &rawImg, &alloc, nullptr);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("[Astral::OwnedImage] vmaCreateImage basarisiz! VkResult: " + std::to_string(res));
    }

    m_Image = rawImg;
    m_Allocation = alloc;

    try {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image = m_Image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        m_View = device.createImageViewUnique(viewInfo);
    } catch (...) {
        Release();
        throw;
    }
}

OwnedImage::~OwnedImage() {
    Release();
}

void OwnedImage::Release() noexcept {
    m_View.reset();
    if (m_Image && m_Allocator) {
        vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
        m_Image = VK_NULL_HANDLE;
        m_Allocation = VK_NULL_HANDLE;
    }
    m_Allocator = VK_NULL_HANDLE;
    m_Device = vk::Device{};
    m_Format = vk::Format::eUndefined;
    m_Extent = vk::Extent2D{};
}

OwnedImage::OwnedImage(OwnedImage&& other) noexcept
    : m_Allocator(other.m_Allocator),
      m_Device(other.m_Device),
      m_Image(other.m_Image),
      m_Allocation(other.m_Allocation),
      m_View(std::move(other.m_View)),
      m_Format(other.m_Format),
      m_Extent(other.m_Extent) {
    other.m_Allocator = VK_NULL_HANDLE;
    other.m_Device = vk::Device{};
    other.m_Image = VK_NULL_HANDLE;
    other.m_Allocation = VK_NULL_HANDLE;
    other.m_Format = vk::Format::eUndefined;
    other.m_Extent = vk::Extent2D{};
}

OwnedImage& OwnedImage::operator=(OwnedImage&& other) noexcept {
    if (this != &other) {
        Release();
        m_Allocator = other.m_Allocator;
        m_Device = other.m_Device;
        m_Image = other.m_Image;
        m_Allocation = other.m_Allocation;
        m_View = std::move(other.m_View);
        m_Format = other.m_Format;
        m_Extent = other.m_Extent;

        other.m_Allocator = VK_NULL_HANDLE;
        other.m_Device = vk::Device{};
        other.m_Image = VK_NULL_HANDLE;
        other.m_Allocation = VK_NULL_HANDLE;
        other.m_Format = vk::Format::eUndefined;
        other.m_Extent = vk::Extent2D{};
    }
    return *this;
}

// =============================================================================
// RenderTargets Implementasyonu
// =============================================================================

RenderTargets::RenderTargets(VulkanContext& context, uint32_t width, uint32_t height)
    : m_Width(width), m_Height(height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("[Astral::RenderTargets] Gecersiz boyut: Genislik ve yukseklik 0 olamaz.");
    }

    auto makeOwnedImage = [&](vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect) {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent = vk::Extent3D{ width, height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = usage;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        return OwnedImage(context.GetAllocator(), context.GetDevice(), imageInfo, allocCreateInfo, aspect);
    };

    // 1. Output / Storage Image (Swapchain blit veya ImGui, RGBA8_UNORM)
    m_StorageImage = makeOwnedImage(
        vk::Format::eR8G8B8A8Unorm,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor
    );

    // 2. Raw Color Image (Linear HDR, RGBA16_SFLOAT)
    m_RawColorImage = makeOwnedImage(
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
        vk::ImageAspectFlagBits::eColor
    );

    // 3 & 4. Ping-pong TAA History Color Images (Linear HDR, RGBA16_SFLOAT)
    for (int i = 0; i < 2; ++i) {
        m_HistoryImages[i] = makeOwnedImage(
            vk::Format::eR16G16B16A16Sfloat,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            vk::ImageAspectFlagBits::eColor
        );
    }

    // 5 & 6. Ping-pong TAA History Extra Images (RGBA32_UINT)
    for (int i = 0; i < 2; ++i) {
        m_HistoryExtraImages[i] = makeOwnedImage(
            vk::Format::eR32G32B32A32Uint,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            vk::ImageAspectFlagBits::eColor
        );
    }

    // 7-11. G-Buffer Images
    m_GBufAlbedo = makeOwnedImage(
        vk::Format::eR8G8B8A8Unorm,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor
    );

    m_GBufNormal = makeOwnedImage(
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
        vk::ImageAspectFlagBits::eColor
    );

    m_GBufMaterial = makeOwnedImage(
        vk::Format::eR32G32B32A32Uint,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
        vk::ImageAspectFlagBits::eColor
    );

    m_GBufDepth = makeOwnedImage(
        vk::Format::eR32Sfloat,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
        vk::ImageAspectFlagBits::eColor
    );

    m_GBufMotion = makeOwnedImage(
        vk::Format::eR16G16Sfloat,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
        vk::ImageAspectFlagBits::eColor
    );

    // Tum goruntuleri guvenli eGeneral duzenine gecir ve temizle
    context.ExecuteImmediate([&](vk::CommandBuffer cmd) {
        auto makeInitBarrier = [](vk::Image img) {
            vk::ImageMemoryBarrier b{};
            b.oldLayout = vk::ImageLayout::eUndefined;
            b.newLayout = vk::ImageLayout::eGeneral;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = img;
            b.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
            b.srcAccessMask = {};
            b.dstAccessMask = vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
            return b;
        };

        std::array<vk::ImageMemoryBarrier, 11> initBarriers = {
            makeInitBarrier(m_StorageImage.GetImage()),
            makeInitBarrier(m_RawColorImage.GetImage()),
            makeInitBarrier(m_HistoryImages[0].GetImage()),
            makeInitBarrier(m_HistoryImages[1].GetImage()),
            makeInitBarrier(m_HistoryExtraImages[0].GetImage()),
            makeInitBarrier(m_HistoryExtraImages[1].GetImage()),
            makeInitBarrier(m_GBufAlbedo.GetImage()),
            makeInitBarrier(m_GBufNormal.GetImage()),
            makeInitBarrier(m_GBufMaterial.GetImage()),
            makeInitBarrier(m_GBufDepth.GetImage()),
            makeInitBarrier(m_GBufMotion.GetImage())
        };

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader,
            {},
            0, nullptr, 0, nullptr,
            static_cast<uint32_t>(initBarriers.size()), initBarriers.data()
        );

        // Tarihce, motion vector ve raw color hedeflerini 0 ile temizle (NaN / cop veri onleme)
        vk::ClearColorValue zeroColor(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f });
        vk::ClearColorValue zeroUint(std::array<uint32_t, 4>{ 0u, 0u, 0u, 0u });
        vk::ImageSubresourceRange clearRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        cmd.clearColorImage(m_GBufMotion.GetImage(), vk::ImageLayout::eGeneral, zeroColor, clearRange);
        cmd.clearColorImage(m_HistoryImages[0].GetImage(), vk::ImageLayout::eGeneral, zeroColor, clearRange);
        cmd.clearColorImage(m_HistoryImages[1].GetImage(), vk::ImageLayout::eGeneral, zeroColor, clearRange);
        cmd.clearColorImage(m_HistoryExtraImages[0].GetImage(), vk::ImageLayout::eGeneral, zeroUint, clearRange);
        cmd.clearColorImage(m_HistoryExtraImages[1].GetImage(), vk::ImageLayout::eGeneral, zeroUint, clearRange);
        cmd.clearColorImage(m_RawColorImage.GetImage(), vk::ImageLayout::eGeneral, zeroColor, clearRange);

        vk::MemoryBarrier clearDoneBarrier{};
        clearDoneBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        clearDoneBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader,
            {},
            1, &clearDoneBarrier,
            0, nullptr,
            0, nullptr
        );
    });
}

GBufferViews RenderTargets::GetGBuffer() const noexcept {
    GBufferViews views;
    views.albedo = m_GBufAlbedo.GetRef();
    views.normal = m_GBufNormal.GetRef();
    views.material = m_GBufMaterial.GetRef();
    views.depth = m_GBufDepth.GetRef();
    views.motion = m_GBufMotion.GetRef();
    return views;
}

} // namespace Astral
