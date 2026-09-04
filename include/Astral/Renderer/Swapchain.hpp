#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <cstdint>

namespace Astral {

class Swapchain {
public:
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    Swapchain(vk::Device device, vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, uint32_t width, uint32_t height);
    ~Swapchain() = default;

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) noexcept = default;
    Swapchain& operator=(Swapchain&&) noexcept = default;

    [[nodiscard]] vk::SwapchainKHR GetSwapchain() const noexcept { return m_SwapChain.get(); }
    [[nodiscard]] vk::Format GetFormat() const noexcept { return m_SwapChainImageFormat; }
    [[nodiscard]] vk::Extent2D GetExtent() const noexcept { return m_SwapChainExtent; }
    [[nodiscard]] const std::vector<vk::ImageView>& GetImageViews() const noexcept { return m_SwapChainImageViews; }
    [[nodiscard]] const std::vector<vk::Image>& GetImages() const noexcept { return m_SwapChainImages; }

    static SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

private:
    vk::Device m_Device;
    vk::UniqueSwapchainKHR m_SwapChain;
    std::vector<vk::Image> m_SwapChainImages;
    vk::Format m_SwapChainImageFormat = vk::Format::eB8G8R8A8Unorm;
    vk::Extent2D m_SwapChainExtent;
    std::vector<vk::UniqueImageView> m_SwapChainImageViewsUnique;
    std::vector<vk::ImageView> m_SwapChainImageViews;

    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
};

} // namespace Astral
