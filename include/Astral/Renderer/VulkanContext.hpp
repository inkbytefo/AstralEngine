#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace Astral {

class Window;

struct QueueFamilyIndices {
    uint32_t graphicsFamily = 0;
    uint32_t presentFamily = 0;
    bool hasGraphicsFamily = false;
    bool hasPresentFamily = false;

    bool IsComplete() const { return hasGraphicsFamily && hasPresentFamily; }
};

class VulkanContext {
public:
    explicit VulkanContext(Window& window, bool enableValidation = true);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    vk::Instance GetInstance() const { return m_Instance.get(); }
    vk::PhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    vk::Device GetDevice() const { return m_Device.get(); }
    vk::Queue GetGraphicsQueue() const { return m_GraphicsQueue; }
    vk::Queue GetPresentQueue() const { return m_PresentQueue; }
    vk::SurfaceKHR GetSurface() const { return m_Surface.get(); }
    const QueueFamilyIndices& GetQueueFamilies() const { return m_QueueIndices; }

    bool IsValidationEnabled() const { return m_EnableValidation; }

    void WaitIdle();

private:
    Window& m_Window;
    bool m_EnableValidation = true;

    vk::UniqueInstance m_Instance;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    vk::UniqueSurfaceKHR m_Surface;
    vk::PhysicalDevice m_PhysicalDevice = nullptr;
    vk::UniqueDevice m_Device;
    vk::Queue m_GraphicsQueue;
    vk::Queue m_PresentQueue;
    QueueFamilyIndices m_QueueIndices;

    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();

    QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device);
    bool IsDeviceSuitable(vk::PhysicalDevice device);
    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );
};

} // namespace Astral
