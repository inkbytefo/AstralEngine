#pragma once

#include "Core/Logger.h"
#include "VulkanDevice.h"
#include "VulkanFramebuffer.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <optional>

namespace AstralEngine {

/**
 * @class VulkanSwapchain
 * @brief Vulkan swapchain yönetimi için temel sınıf
 * 
 * Bu sınıf, swapchain oluşturma, yönetme ve yeniden oluşturma işlemlerini
 * gerçekleştirir. Renkli resimler, derinlik tamponu, render pass ve
 * framebuffer'ları yönetir.
 */
class VulkanSwapchain {
public:
    /**
     * @brief Swapchain destek detaylarını tutan yapı
     */
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;     ///< Surface yetenekleri
        std::vector<VkSurfaceFormatKHR> formats;  ///< Desteklenen formatlar
        std::vector<VkPresentModeKHR> presentModes; ///< Desteklenen sunum modları
    };

    VulkanSwapchain();
    ~VulkanSwapchain();

    // Lifecycle management
    bool Initialize(VulkanDevice* device);
    void Shutdown();
    bool Recreate();

    // Getter methods
    VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
    VkRenderPass GetRenderPass() const { return m_renderPass; }
    VkFramebuffer GetFramebuffer(uint32_t index) const;
    VkImageView GetImageView(uint32_t index) const;
    uint32_t GetImageCount() const { return static_cast<uint32_t>(m_swapchainImages.size()); }
    VkExtent2D GetExtent() const { return m_swapchainExtent; }
    VkFormat GetImageFormat() const { return m_swapchainImageFormat; }
    VkFormat GetDepthFormat() const { return m_depthFormat; }
    
    // State management
    bool IsInitialized() const { return m_isInitialized; }
    
    // Error handling
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Swapchain support query methods
    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Resource creation methods
    bool CreateSwapchain();
    bool CreateImageViews();
    bool CreateDepthResources();
    bool CreateRenderPass();
    bool CreateFramebuffers();

    // Cleanup method
    void Cleanup();

    // Helper methods
    bool CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                    VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                    VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    // Error handling
    void HandleVulkanError(VkResult result, const std::string& operation);
    void SetError(const std::string& error);

    // Member variables
    VulkanDevice* m_device = nullptr;
    std::string m_lastError;
    
    // Swapchain resources
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent{};
    
    // Depth resources
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    
    // Render pass and framebuffers
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<std::unique_ptr<VulkanFramebuffer>> m_framebuffers;
    
    // State management
    bool m_isInitialized = false;
};

} // namespace AstralEngine
