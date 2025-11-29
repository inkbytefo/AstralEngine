# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Graphics Abstraction Layer
#  *
#  * Header File: GraphicsDevice.h
#  * Purpose: Manages low-level Vulkan device and instance creation.
#  * Author: Jules (AI Assistant)
#  * Version: 1.0.0
#  * Date: 2025-09-15
#  * Dependencies: Vulkan, C++20
#  **********************************************************************/
#pragma once

#include <vector>
#include <memory>
#include <optional>
#include "Vulkan/VulkanSwapchain.h"
#include "Vulkan/VulkanPipeline.h"
#include "Vulkan/VulkanCommandManager.h"

#ifdef ASTRAL_USE_VULKAN
    #include <vulkan/vulkan.h>
#endif

namespace AstralEngine {

class Window;
class VulkanSwapchain;
class VulkanPipeline;
class VulkanCommandManager;

/**
 * @brief Vulkan GraphicsDevice wrapper class
 *
 * Manages the Vulkan instance, device, queues, and other core resources.
 * It is designed to work in conjunction with the SDL3 Window class.
 */
class GraphicsDevice {
public:
    GraphicsDevice();
    ~GraphicsDevice();

    // Non-copyable
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    // Lifecycle
    bool Initialize(Window* window);
    void Shutdown();

    // Getters for Vulkan handles
#ifdef ASTRAL_USE_VULKAN
    VkInstance GetInstance() const { return m_instance; }
    VkDevice GetDevice() const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkSurfaceKHR GetSurface() const { return m_surface; }

    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetPresentQueue() const { return m_presentQueue; }

    uint32_t GetGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    uint32_t GetPresentQueueFamily() const { return m_presentQueueFamily; }

    // Accessors for owned components
    VulkanSwapchain* GetSwapchain() const { return m_swapchain.get(); }
    VulkanPipeline* GetPipeline() const { return m_pipeline.get(); }
    VulkanCommandManager* GetCommandManager() const { return m_commandManager.get(); }
    VkRenderPass GetRenderPass() const { return m_renderPass; }
#endif

    bool IsValidationLayersEnabled() const { return m_validationLayersEnabled; }

private:
    // Initialization steps
    bool CreateInstance();
    bool SetupDebugMessenger();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();

    // Helper methods
    void CreateRenderPass();
    bool CheckValidationLayerSupport();

#ifdef ASTRAL_USE_VULKAN
    // Core Vulkan objects
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    // Debug messenger
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    // Queues
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;
#endif

    Window* m_window = nullptr; // Non-owning pointer to the window
    bool m_initialized = false;
    bool m_validationLayersEnabled = false;

    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<VulkanCommandManager> m_commandManager;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};

} // namespace AstralEngine
