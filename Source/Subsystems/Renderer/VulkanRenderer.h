#pragma once

#include "IRenderer.h"
#include "Core/ISubsystem.h"
#include "Core/Engine.h"
#include "Events/Event.h"
#include "RenderCommandQueue.h"
#include "Vulkan/VulkanGraphicsContext.h"
#include "Vulkan/VulkanCommandBufferManager.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>

// Forward declarations
namespace AstralEngine {
class VulkanShader;
class VulkanPipeline;
class VulkanBuffer;
class Camera;
}

// Constants
const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

namespace AstralEngine {

/**
 * @class VulkanRenderer
 * @brief Vulkan 1.4 Renderer implementasyonu
 * 
 * Astral Engine için yüksek performanslı, modern Vulkan 1.4 renderer'ı.
 * IRenderer arayüzünü implemente eder ve komut kuyruğu sistemini kullanır.
 * ISubsystem'den türetilmiş IRenderer sayesinde tutarlı yaşam döngüsü sağlanır.
 */
class VulkanRenderer : public IRenderer {
public:
    struct Config {
        std::string applicationName = "Astral Engine";
        uint32_t applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        std::string engineName = "Astral Engine";
        uint32_t engineVersion = VK_MAKE_VERSION(1, 0, 0);
        
        bool enableValidationLayers = true;
        uint32_t windowWidth = 1920;
        uint32_t windowHeight = 1080;
    };

    VulkanRenderer();
    virtual ~VulkanRenderer();

    // ISubsystem interface implementation (via IRenderer inheritance)
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "VulkanRenderer"; }

    // IRenderer interface implementation
    // Initialize() ve Shutdown() metodları OnInitialize() ve OnShutdown() içinde çağrılacak
    bool Initialize() override;
    void Shutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Submit(const RenderCommand& command) override;
    void SubmitCommands(const std::vector<RenderCommand>& commands) override;
    bool IsInitialized() const override { return m_isInitialized; }
    RendererAPI GetAPI() const override { return RendererAPI::Vulkan; }
    void SetClearColor(float r, float g, float b, float a) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    
    // Legacy getters for backward compatibility
    VulkanGraphicsContext* GetGraphicsContext() const { return m_graphicsContext.get(); }
    VulkanCommandBufferManager* GetCommandBufferManager() const { return m_commandBufferManager.get(); }
    
    // Configuration
    const Config& GetConfig() const { return m_config; }
    void UpdateConfig(const Config& config);
    
    // Frame management
    uint32_t GetCurrentFrameIndex() const { return m_currentFrame; }
    uint32_t GetFrameCount() const { return m_frameCount; }
    float GetFrameTime() const { return m_frameTime; }
    float GetFPS() const { return m_fps; }

private:
    // Lifecycle management
    bool InitializeCoreComponents(Engine* owner);
    void ShutdownCoreComponents();
    
    // Frame management helpers
    bool BeginFrameInternal();
    bool EndFrameInternal();
    bool PresentInternal();
    
    // Performance monitoring
    void UpdatePerformanceMetrics(float deltaTime);
    
    // Error handling
    void HandleVulkanError(VkResult result, const std::string& operation);
    
    // Swapchain recreation and error recovery
    bool HandleSwapchainRecreation();
    void SynchronizeFrameIndex();
    
    // Component initialization helpers
    bool InitializeShaders(Engine* owner);
    bool InitializePipeline();
    bool InitializeVertexBuffer();
    bool CreateFallbackPipeline(); // Fallback pipeline for debugging
    
    // Descriptor set and uniform buffer helpers
    bool CreateDescriptorSetLayout();
    bool CreateDescriptorPool();
    bool CreateDescriptorSets();
    bool CreateUniformBuffers();
    bool UpdateDescriptorSets();
    
    // Command processing
    void ProcessRenderCommands(const std::vector<RenderCommand>& commands);
    void ExecuteRenderCommand(const RenderCommand& command);
    
    // Rendering helpers
    void DrawFrame();
    void RecordCommandBuffer(uint32_t imageIndex);
    void UpdateUniformBuffer();
    
    // Member variables
    Config m_config;
    
    // Core components (refactored)
    std::unique_ptr<VulkanGraphicsContext> m_graphicsContext;
    std::unique_ptr<VulkanCommandBufferManager> m_commandBufferManager;
    std::unique_ptr<RenderCommandQueue> m_commandQueue;
    
    // Rendering components
    std::unique_ptr<VulkanShader> m_vertexShader;
    std::unique_ptr<VulkanShader> m_fragmentShader;
    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    
    // Descriptor set and uniform buffer components
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    std::vector<std::unique_ptr<VulkanBuffer>> m_uniformBuffers;
    
    // Frame management
    uint32_t m_currentFrame = 0;
    uint32_t m_frameCount = 0;
    float m_frameTime = 0.0f;
    float m_fps = 0.0f;
    
    // Render state
    float m_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    VkViewport m_currentViewport = {};
    VkRect2D m_currentScissor = {};
    
    // State management
    bool m_isInitialized = false;
    bool m_isFrameStarted = false;
    
    // 3D rendering components
    std::unique_ptr<Camera> m_camera;
    float m_startTime = 0.0f;
};

} // namespace AstralEngine
