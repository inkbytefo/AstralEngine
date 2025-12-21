#pragma once

#include "../IRHIDevice.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>
#include <optional>

namespace AstralEngine {

class Window;

class VulkanDevice : public IRHIDevice {
public:
    VulkanDevice(Window* window);
    ~VulkanDevice() override;

    bool Initialize() override;
    void Shutdown() override;

    std::shared_ptr<IRHIBuffer> CreateBuffer(uint64_t size, RHIBufferUsage usage, RHIMemoryProperty memoryProperties) override;
    std::shared_ptr<IRHITexture> CreateTexture2D(uint32_t width, uint32_t height, RHIFormat format, RHITextureUsage usage) override;
    std::shared_ptr<IRHIShader> CreateShader(RHIShaderStage stage, std::span<const uint8_t> code) override;
    std::shared_ptr<IRHIPipeline> CreateGraphicsPipeline(const RHIPipelineStateDescriptor& descriptor) override;
    std::shared_ptr<IRHICommandList> CreateCommandList() override;
    void SubmitCommandList(IRHICommandList* commandList) override;

    void BeginFrame() override;
    void Present() override;
    IRHITexture* GetCurrentBackBuffer() override;
    void WaitIdle() override;

    // Getters for internal use
    VkDevice GetVkDevice() const { return m_device; }
    VkInstance GetInstance() const { return m_instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    uint32_t GetGraphicsQueueFamilyIndex() const { return m_graphicsQueueFamilyIndex; }
    VkRenderPass GetSwapchainRenderPass() const { return m_renderPass; } // Simplified: usually we want dynamic rendering
    VkExtent2D GetSwapchainExtent() const { return m_swapchainExtent; }
    uint32_t GetSwapchainImageCount() const { return static_cast<uint32_t>(m_swapchainImages.size()); }
    VmaAllocator GetAllocator() const { return m_allocator; }
    
    uint32_t GetCurrentImageIndex() const { return m_imageIndex; }
    VkFramebuffer GetFramebuffer(uint32_t index) const { return m_swapchainFramebuffers[index]; }

private:
    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateAllocator();
    void CreateSwapchain();
    void CreateImageViews();
    void CreateRenderPass(); // Legacy support or for simple start
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateSyncObjects();

    Window* m_window;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    
    uint32_t m_graphicsQueueFamilyIndex = ~0u;
    uint32_t m_presentQueueFamilyIndex = ~0u;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    VkFormat m_swapchainImageFormat;
    VkExtent2D m_swapchainExtent;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<std::shared_ptr<IRHITexture>> m_swapchainTextures; // Wrappers

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;

    std::vector<VkCommandPool> m_commandPools; // Per-frame command pools
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    // Frame synchronization
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    bool m_frameValid = false; // Indicates if the current frame successfully acquired an image
};

} // namespace AstralEngine
