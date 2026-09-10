#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <array>
#include <cstdint>
#include <memory>

namespace Astral {

class VulkanContext;

/// Vulkan goruntu ve gorunumunu kapsayan hafif referans yapisi (Non-owning)
struct ImageViewRef {
    vk::Image image{};
    vk::ImageView view{};
    vk::Format format = vk::Format::eUndefined;
    vk::Extent2D extent{};
};

/// G-Buffer aşaması render hedefleri görünümleri
struct GBufferViews {
    ImageViewRef albedo;
    ImageViewRef normal;
    ImageViewRef material;
    ImageViewRef depth;
    ImageViewRef motion;
};

/// VmaAllocator, VkImage, VmaAllocation ve vk::ImageView sahipligini tek nesnede toplayan RAII sinifi.
/// Kopyalama yasaktir, tasima (move) kaynak nesneyi sifirlar. Destructor once view'i sonra image'i yok eder.
class OwnedImage {
public:
    OwnedImage() = default;
    OwnedImage(VmaAllocator allocator,
               vk::Device device,
               const vk::ImageCreateInfo& imageInfo,
               const VmaAllocationCreateInfo& allocInfo,
               vk::ImageAspectFlags aspectFlags);
    ~OwnedImage();

    OwnedImage(const OwnedImage&) = delete;
    OwnedImage& operator=(const OwnedImage&) = delete;
    OwnedImage(OwnedImage&& other) noexcept;
    OwnedImage& operator=(OwnedImage&& other) noexcept;

    [[nodiscard]] vk::Image GetImage() const noexcept { return m_Image; }
    [[nodiscard]] vk::ImageView GetView() const noexcept { return m_View.get(); }
    [[nodiscard]] vk::Format GetFormat() const noexcept { return m_Format; }
    [[nodiscard]] vk::Extent2D GetExtent() const noexcept { return m_Extent; }
    [[nodiscard]] ImageViewRef GetRef() const noexcept {
        return ImageViewRef{ m_Image, m_View.get(), m_Format, m_Extent };
    }
    [[nodiscard]] bool IsValid() const noexcept { return static_cast<bool>(m_Image); }

    void Release() noexcept;

private:
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    vk::Device m_Device{};
    vk::Image m_Image{};
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    vk::UniqueImageView m_View{};
    vk::Format m_Format = vk::Format::eUndefined;
    vk::Extent2D m_Extent{};
};

/// Tum render hedeflerini (G-Buffer, HDR, Cikti, Tarihce) tek bir atomik pakette toplayan RAII sinifi.
/// Olusturma sirasinda tum 11 goruntu tahsis edilir ve ilk eGeneral layout gecisleri ile temizleme islemleri yapilir.
class RenderTargets {
public:
    RenderTargets(VulkanContext& context, uint32_t width, uint32_t height);
    ~RenderTargets() = default;

    RenderTargets(const RenderTargets&) = delete;
    RenderTargets& operator=(const RenderTargets&) = delete;
    RenderTargets(RenderTargets&&) noexcept = default;
    RenderTargets& operator=(RenderTargets&&) noexcept = default;

    [[nodiscard]] GBufferViews GetGBuffer() const noexcept;
    [[nodiscard]] ImageViewRef GetOutput() const noexcept { return m_StorageImage.GetRef(); }
    [[nodiscard]] ImageViewRef GetRawColor() const noexcept { return m_RawColorImage.GetRef(); }
    [[nodiscard]] ImageViewRef GetHistoryColor(uint32_t index) const noexcept {
        return m_HistoryImages[index % 2].GetRef();
    }
    [[nodiscard]] ImageViewRef GetHistoryExtra(uint32_t index) const noexcept {
        return m_HistoryExtraImages[index % 2].GetRef();
    }

    [[nodiscard]] uint32_t GetWidth() const noexcept { return m_Width; }
    [[nodiscard]] uint32_t GetHeight() const noexcept { return m_Height; }
    [[nodiscard]] vk::Extent2D GetExtent() const noexcept { return vk::Extent2D{ m_Width, m_Height }; }

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    // 1. Storage / Output Image (Swapchain blit veya ImGui, RGBA8_UNORM)
    OwnedImage m_StorageImage;

    // 2. Raw Color Image (Linear HDR, RGBA16_SFLOAT)
    OwnedImage m_RawColorImage;

    // 3 & 4. Ping-pong TAA History Color Images (Linear HDR, RGBA16_SFLOAT)
    std::array<OwnedImage, 2> m_HistoryImages;

    // 5 & 6. Ping-pong TAA History Extra Images (RGBA32_UINT)
    std::array<OwnedImage, 2> m_HistoryExtraImages;

    // 7-11. G-Buffer Images
    OwnedImage m_GBufAlbedo;    // R8G8B8A8_UNORM
    OwnedImage m_GBufNormal;    // R16G16B16A16_SFLOAT
    OwnedImage m_GBufMaterial;  // R32G32B32A32_UINT
    OwnedImage m_GBufDepth;     // R32_SFLOAT
    OwnedImage m_GBufMotion;    // R16G16_SFLOAT
};

} // namespace Astral
