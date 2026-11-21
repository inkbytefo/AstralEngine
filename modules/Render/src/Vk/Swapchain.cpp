#include "Vk/Swapchain.hpp"
#include "Vk/Device.hpp"

#include <algorithm>

namespace Astral::Render::Vk {

Swapchain::Swapchain(Device& device, vk::SurfaceKHR surface, uint32_t width, uint32_t height)
    : m_Device(device), m_Surface(surface) {
    if (!device.Physical().getSurfaceSupportKHR(device.QueueFamily(), surface))
        throw std::runtime_error("the renderer's queue cannot present to this surface");
    Create(width, height);
}

Swapchain::~Swapchain() {
    m_Device.Get().waitIdle();
    Destroy();
    m_Device.Instance().destroySurfaceKHR(m_Surface);
}

void Swapchain::Recreate(uint32_t width, uint32_t height) {
    m_Device.Get().waitIdle();
    Destroy();
    Create(width, height);
}

void Swapchain::Create(uint32_t width, uint32_t height) {
    vk::PhysicalDevice physical = m_Device.Physical();
    auto caps = physical.getSurfaceCapabilitiesKHR(m_Surface);
    auto formats = physical.getSurfaceFormatsKHR(m_Surface);

    // UNORM target: the tonemapper already writes display-encoded (sRGB) values.
    vk::SurfaceFormatKHR chosen = formats.front();
    for (const auto& f : formats) {
        if ((f.format == vk::Format::eB8G8R8A8Unorm || f.format == vk::Format::eR8G8B8A8Unorm) &&
            f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            chosen = f;
            break;
        }
    }
    m_Format = chosen.format;

    if (caps.currentExtent.width != UINT32_MAX) {
        m_Extent = caps.currentExtent;
    } else {
        m_Extent = vk::Extent2D{std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
                                std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)};
    }

    uint32_t imageCount = std::max(3u, caps.minImageCount);
    if (caps.maxImageCount) imageCount = std::min(imageCount, caps.maxImageCount);

    vk::SwapchainCreateInfoKHR info;
    info.surface = m_Surface;
    info.minImageCount = imageCount;
    info.imageFormat = chosen.format;
    info.imageColorSpace = chosen.colorSpace;
    info.imageExtent = m_Extent;
    info.imageArrayLayers = 1;
    info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    info.presentMode = vk::PresentModeKHR::eFifo; // always available, no tearing
    info.clipped = VK_TRUE;
    m_Swapchain = m_Device.Get().createSwapchainKHR(info);

    m_Images = m_Device.Get().getSwapchainImagesKHR(m_Swapchain);
    m_States.assign(m_Images.size(), {});
    for (vk::Image image : m_Images) {
        vk::ImageViewCreateInfo view({}, image, vk::ImageViewType::e2D, m_Format, {},
                                     {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        m_Views.push_back(m_Device.Get().createImageView(view));
        m_RenderFinished.push_back(m_Device.Get().createSemaphore({}));
    }
}

void Swapchain::Destroy() {
    for (auto v : m_Views) m_Device.Get().destroyImageView(v);
    for (auto s : m_RenderFinished) m_Device.Get().destroySemaphore(s);
    m_Views.clear();
    m_RenderFinished.clear();
    m_Images.clear();
    if (m_Swapchain) m_Device.Get().destroySwapchainKHR(m_Swapchain);
    m_Swapchain = nullptr;
}

bool Swapchain::Acquire(vk::Semaphore signal, uint32_t& imageIndex) {
    VkResult r = VULKAN_HPP_DEFAULT_DISPATCHER.vkAcquireNextImageKHR(m_Device.Get(), m_Swapchain, UINT64_MAX, signal,
                                                                     VK_NULL_HANDLE, &imageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) return false;
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) Check(r, "vkAcquireNextImageKHR");
    return true;
}

bool Swapchain::Present(vk::Queue queue, uint32_t imageIndex) {
    VkSemaphore wait = m_RenderFinished[imageIndex];
    VkSwapchainKHR swapchain = m_Swapchain;
    VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &wait;
    info.swapchainCount = 1;
    info.pSwapchains = &swapchain;
    info.pImageIndices = &imageIndex;
    VkResult r = VULKAN_HPP_DEFAULT_DISPATCHER.vkQueuePresentKHR(queue, &info);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) return false;
    Check(r, "vkQueuePresentKHR");
    return true;
}

} // namespace Astral::Render::Vk
