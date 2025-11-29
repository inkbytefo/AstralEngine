#pragma once

#include "../../../Core/Logger.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace AstralEngine {

class GraphicsDevice;
class Window;

class VulkanSwapchain {
public:
    VulkanSwapchain(GraphicsDevice* device, Window* window);
    ~VulkanSwapchain();

    void Initialize(VkRenderPass renderPass);
    void Shutdown();

    VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
    const std::vector<VkImage>& GetImages() const { return m_images; }
    const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
    const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_framebuffers; }
    VkFormat GetImageFormat() const { return m_imageFormat; }
    VkExtent2D GetExtent() const { return m_extent; }

private:
    void CreateSwapchain();
    void CreateImageViews();
    void CreateFramebuffers(VkRenderPass renderPass);

    // Helper functions for creation
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    GraphicsDevice* m_device; // Non-owning
    Window* m_window; // Non-owning

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    VkFormat m_imageFormat;
    VkExtent2D m_extent;
};

}
