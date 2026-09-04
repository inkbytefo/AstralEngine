#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace Astral {

class Window;
class Swapchain;

struct QueueFamilyIndices {
    uint32_t graphicsFamily = 0;
    uint32_t presentFamily = 0;
    bool hasGraphicsFamily = false;
    bool hasPresentFamily = false;

    bool IsComplete() const { return hasGraphicsFamily && hasPresentFamily; }
};

struct VmaImage {
    vk::Image image = nullptr;
    VmaAllocation allocation = VK_NULL_HANDLE;

    [[nodiscard]] vk::Image get() const noexcept { return image; }
    [[nodiscard]] operator vk::Image() const noexcept { return image; }
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(image); }
    void reset() noexcept {
        image = nullptr;
        allocation = VK_NULL_HANDLE;
    }
};

class VulkanContext {
public:
    static constexpr uint32_t QUERY_FRAME_START = 0;
    static constexpr uint32_t QUERY_FRAME_END = 1;
    static constexpr uint32_t QUERY_COUNT = 2;

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

    // Profiling & Timestamps
    float GetTimestampPeriod() const { return m_TimestampPeriod; }
    std::string GetDeviceName() const;
    std::string GetDriverVersionString() const;
    std::string GetVulkanVersionString() const;

    // Frame Command & GPU Profiling
    vk::CommandBuffer BeginFrameCommand();
    void WriteTimestamp(vk::CommandBuffer cmd, vk::PipelineStageFlagBits stage, uint32_t queryIndex);
    void EndAndSubmitFrameCommand();
    double GetLastGpuTimeMs();

    // Memory & VMA
    [[nodiscard]] VmaAllocator GetAllocator() const noexcept { return m_Allocator; }
    VmaTotalStatistics GetMemoryStats() const;

    // Swapchain & Presentation
    void CreateSwapchain();
    void RecreateSwapchain();
    Swapchain* GetSwapchain() const { return m_Swapchain.get(); }
    uint32_t GetCurrentImageIndex() const { return m_CurrentImageIndex; }
    bool AcquireNextImage();
    void PrepareSwapchainImage();
    void EndFrameBlit(vk::Image sourceImage, uint32_t srcWidth, uint32_t srcHeight);
    void EndFramePresent();

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

    // Command & Query Pools
    vk::UniqueCommandPool m_CommandPool;
    vk::UniqueCommandBuffer m_CommandBuffer;
    vk::UniqueFence m_FrameFence;
    vk::UniqueQueryPool m_TimestampQueryPool;
    float m_TimestampPeriod = 1.0f;
    double m_LastGpuTimeMs = 0.0;
    bool m_HasValidGpuTime = false;

    // Swapchain & Synch
    std::unique_ptr<Swapchain> m_Swapchain;
    std::vector<vk::UniqueSemaphore> m_ImageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_RenderFinishedSemaphores;
    uint32_t m_CurrentImageIndex = 0;
    size_t m_CurrentFrame = 0;

    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateCommandPoolAndBuffers();
    void CreateTimestampQueryPool();
    void CreateAllocator();
    void DestroyAllocator();

    VmaAllocator m_Allocator = VK_NULL_HANDLE;

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
