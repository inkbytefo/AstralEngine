#include "VulkanRenderer.h"
#include "Core/VulkanInstance.h"
#include "Core/VulkanDevice.h"
#include "Core/VulkanSwapchain.h"
#include "Shaders/VulkanShader.h"
#include "Shaders/ShaderValidator.h"
#include "Commands/VulkanPipeline.h"
#include "Commands/VulkanCommandPool.h"
#include "Buffers/VulkanBuffer.h"
#include "RendererTypes.h"
#include "Camera.h"
#include "Core/Logger.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Core/Engine.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <filesystem>

namespace AstralEngine {

VulkanRenderer::VulkanRenderer()
: m_currentFrame(0)
, m_frameCount(0)
, m_frameTime(0.0f)
, m_fps(0.0f)
, m_isInitialized(false)
, m_isFrameStarted(false) {

// Set default configuration with validation layers enabled
m_config = Config{};
m_config.enableValidationLayers = true; // Force enable validation layers for debugging
}

VulkanRenderer::~VulkanRenderer() {
    if (m_isInitialized) {
        OnShutdown();
    }
}

void VulkanRenderer::OnInitialize(Engine* owner) {
    if (m_isInitialized) {
        Logger::Error("VulkanRenderer", "VulkanRenderer already initialized");
        return;
    }
    
    try {
        Logger::Info("VulkanRenderer", "Initializing VulkanRenderer...");
        
        // Initialize core components
        if (!InitializeCoreComponents(owner)) {
            Logger::Error("VulkanRenderer", "Failed to initialize core Vulkan components");
            return;
        }
        
        m_isInitialized = true;
        Logger::Info("VulkanRenderer", "VulkanRenderer initialized successfully");
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "VulkanRenderer initialization failed: " + std::string(e.what()));
    }
}

bool VulkanRenderer::Initialize() {
    // IRenderer arayüzü için Initialize - OnInitialize içinde çağrılacak
    // Artık doğrudan OnInitialize tarafından yönetiliyor
    return m_isInitialized;
}

void VulkanRenderer::Shutdown() {
    // IRenderer arayüzü için Shutdown - OnShutdown içinde çağrılacak
    // Artık doğrudan OnShutdown tarafından yönetiliyor
}

void VulkanRenderer::OnUpdate(float deltaTime) {
    Logger::Debug("VulkanRenderer", "OnUpdate called");
    if (!m_isInitialized) {
        Logger::Warning("VulkanRenderer", "OnUpdate called but renderer not initialized");
        return;
    }
    
    try {
        // Update performance metrics
        UpdatePerformanceMetrics(deltaTime);
        
        // DrawFrame handles all frame management internally
        Logger::Debug("VulkanRenderer", "Rendering frame with default triangle");
        DrawFrame();
        
        Logger::Debug("VulkanRenderer", "Frame completed successfully");
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "VulkanRenderer update failed: " + std::string(e.what()));
    }
}

void VulkanRenderer::OnShutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    try {
        Logger::Info("VulkanRenderer", "Shutting down VulkanRenderer...");
        
        // Shutdown core components
        ShutdownCoreComponents();
        
        m_isInitialized = false;
        Logger::Info("VulkanRenderer", "VulkanRenderer shutdown completed");
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "VulkanRenderer shutdown failed: " + std::string(e.what()));
    }
}

void VulkanRenderer::BeginFrame() {
    if (!m_isInitialized || m_isFrameStarted) {
        return;
    }
    
    // Begin frame using command buffer manager
    if (m_commandBufferManager) {
        m_commandBufferManager->BeginFrame();
    }
    
    m_isFrameStarted = true;
}

void VulkanRenderer::EndFrame() {
    if (!m_isInitialized || !m_isFrameStarted) {
        return;
    }
    
    // End frame using command buffer manager
    if (m_commandBufferManager) {
        m_commandBufferManager->EndFrame();
    }
    
    m_isFrameStarted = false;
}

void VulkanRenderer::Present() {
    if (!m_isInitialized) {
        return;
    }
    
    // Present logic will be handled in DrawFrame for now
    // TODO: Implement proper present logic here
}

void VulkanRenderer::Submit(const RenderCommand& command) {
    if (m_commandQueue) {
        m_commandQueue->Push(command);
    }
}

void VulkanRenderer::SubmitCommands(const std::vector<RenderCommand>& commands) {
    if (m_commandQueue) {
        m_commandQueue->Push(commands);
    }
}

void VulkanRenderer::SetClearColor(float r, float g, float b, float a) {
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
}

void VulkanRenderer::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    m_currentViewport.x = static_cast<float>(x);
    m_currentViewport.y = static_cast<float>(y);
    m_currentViewport.width = static_cast<float>(width);
    m_currentViewport.height = static_cast<float>(height);
    m_currentViewport.minDepth = 0.0f;
    m_currentViewport.maxDepth = 1.0f;
    
    m_currentScissor.offset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
    m_currentScissor.extent = {width, height};
}

void VulkanRenderer::UpdateConfig(const Config& config) {
    m_config = config;
    
    // TODO: Reinitialize components if configuration changes significantly
        Logger::Info("VulkanRenderer", "VulkanRenderer configuration updated");
}

bool VulkanRenderer::InitializeCoreComponents(Engine* owner) {
    try {
        // Initialize graphics context
        m_graphicsContext = std::make_unique<VulkanGraphicsContext>();
        VulkanGraphicsContextConfig contextConfig;
        contextConfig.applicationName = m_config.applicationName;
        contextConfig.applicationVersion = m_config.applicationVersion;
        contextConfig.engineName = m_config.engineName;
        contextConfig.engineVersion = m_config.engineVersion;
        contextConfig.enableValidationLayers = m_config.enableValidationLayers;
        contextConfig.windowWidth = m_config.windowWidth;
        contextConfig.windowHeight = m_config.windowHeight;
        
        if (!m_graphicsContext->Initialize(owner, contextConfig)) {
            Logger::Error("VulkanRenderer", "Failed to initialize graphics context: " + m_graphicsContext->GetLastError());
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Graphics context initialized successfully");

        // Set initial viewport and scissor
        VkExtent2D swapchainExtent = m_graphicsContext->GetSwapchainExtent();
        SetViewport(0, 0, swapchainExtent.width, swapchainExtent.height);
        Logger::Info("VulkanRenderer", "Initial viewport set to swapchain extent ({}x{})", swapchainExtent.width, swapchainExtent.height);
        
        // Initialize command buffer manager
        m_commandBufferManager = std::make_unique<VulkanCommandBufferManager>();
        VulkanCommandBufferManagerConfig managerConfig;
        managerConfig.maxFramesInFlight = MAX_FRAMES_IN_FLIGHT;
        managerConfig.enableDebugMarkers = false;
        
        if (!m_commandBufferManager->Initialize(m_graphicsContext->GetDevice(), managerConfig)) {
            Logger::Error("VulkanRenderer", "Failed to initialize command buffer manager: " + m_commandBufferManager->GetLastError());
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Command buffer manager initialized successfully");
        
        // Initialize render command queue
        m_commandQueue = std::make_unique<RenderCommandQueue>();
        
        Logger::Info("VulkanRenderer", "Render command queue initialized successfully");
        
        // Initialize shaders
        if (!InitializeShaders(owner)) {
            Logger::Error("VulkanRenderer", "Failed to initialize shaders");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Shaders initialized successfully");
        
        // Create descriptor set layout (pipeline'dan önce olmalı)
        if (!CreateDescriptorSetLayout()) {
            Logger::Error("VulkanRenderer", "Failed to create descriptor set layout");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Descriptor set layout created successfully");
        
        // Initialize pipeline
        if (!InitializePipeline()) {
            Logger::Error("VulkanRenderer", "Failed to initialize pipeline");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Pipeline initialized successfully");
        
        // Initialize vertex buffer
        if (!InitializeVertexBuffer()) {
            Logger::Error("VulkanRenderer", "Failed to initialize vertex buffer");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Vertex buffer initialized successfully");
        
        // Create descriptor pool
        if (!CreateDescriptorPool()) {
            Logger::Error("VulkanRenderer", "Failed to create descriptor pool");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Descriptor pool created successfully");
        
        // Create uniform buffers
        if (!CreateUniformBuffers()) {
            Logger::Error("VulkanRenderer", "Failed to create uniform buffers");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Uniform buffers created successfully");
        
        // Create descriptor sets
        if (!CreateDescriptorSets()) {
            Logger::Error("VulkanRenderer", "Failed to create descriptor sets");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Descriptor sets created successfully");
        
        // Update descriptor sets
        if (!UpdateDescriptorSets()) {
            Logger::Error("VulkanRenderer", "Failed to update descriptor sets");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Descriptor sets updated successfully");
        
        // Initialize camera
        m_camera = std::make_unique<Camera>();
        Camera::Config cameraConfig;
        cameraConfig.position = glm::vec3(0.0f, 0.0f, 2.0f);
        cameraConfig.target = glm::vec3(0.0f, 0.0f, 0.0f);
        cameraConfig.up = glm::vec3(0.0f, 1.0f, 0.0f);
        cameraConfig.fov = 45.0f;
        cameraConfig.aspectRatio = static_cast<float>(m_config.windowWidth) / static_cast<float>(m_config.windowHeight);
        cameraConfig.nearPlane = 0.1f;
        cameraConfig.farPlane = 100.0f;
        
        m_camera->Initialize(cameraConfig);
        
        Logger::Info("VulkanRenderer", "Camera initialized successfully");
        
        // Set start time for animation
        m_startTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        Logger::Info("VulkanRenderer", "Start time set for animation");
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Core component initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanRenderer::ShutdownCoreComponents() {
    try {
        // Shutdown in reverse order of initialization
        
        // Wait for device to finish operations
        if (m_graphicsContext && m_graphicsContext->GetVkDevice() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_graphicsContext->GetVkDevice());
        }
        
        // Shutdown camera
        if (m_camera) {
            m_camera->Shutdown();
            m_camera.reset();
        }
        
        // Shutdown uniform buffers
        for (auto& buffer : m_uniformBuffers) {
            if (buffer) {
                buffer->Shutdown();
                buffer.reset();
            }
        }
        m_uniformBuffers.clear();
        
        // Destroy descriptor pool
        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_graphicsContext->GetVkDevice(), m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        
        // Destroy descriptor set layout
        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_graphicsContext->GetVkDevice(), m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }
        
        // Shutdown command queue
        if (m_commandQueue) {
            m_commandQueue.reset();
        }
        
        // Shutdown command buffer manager
        if (m_commandBufferManager) {
            m_commandBufferManager->Shutdown();
            m_commandBufferManager.reset();
        }
        
        // Shutdown graphics context
        if (m_graphicsContext) {
            m_graphicsContext->Shutdown();
            m_graphicsContext.reset();
        }
        
        // Shutdown vertex buffer
        if (m_vertexBuffer) {
            m_vertexBuffer->Shutdown();
            m_vertexBuffer.reset();
        }
        
        // Shutdown pipeline
        if (m_pipeline) {
            m_pipeline->Shutdown();
            m_pipeline.reset();
        }
        
        // Shutdown shaders
        if (m_vertexShader) {
            m_vertexShader->Shutdown();
            m_vertexShader.reset();
        }
        if (m_fragmentShader) {
            m_fragmentShader->Shutdown();
            m_fragmentShader.reset();
        }
        
        Logger::Info("VulkanRenderer", "Core components shutdown completed");
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Core component shutdown failed: " + std::string(e.what()));
    }
}

bool VulkanRenderer::BeginFrameInternal() {
    // TODO: Implement actual frame begin logic
    m_isFrameStarted = true;
    return true;
}

bool VulkanRenderer::EndFrameInternal() {
    // TODO: Implement actual frame end logic
    m_isFrameStarted = false;
    return true;
}

bool VulkanRenderer::PresentInternal() {
    // TODO: Implement actual presentation logic
    return true;
}

void VulkanRenderer::UpdatePerformanceMetrics(float deltaTime) {
    m_frameTime = deltaTime;
    m_frameCount++;
    
    // Calculate FPS (simple moving average)
    static const float fpsUpdateInterval = 1.0f; // Update FPS every second
    static float fpsAccumulator = 0.0f;
    static uint32_t fpsFrameCount = 0;
    
    fpsAccumulator += deltaTime;
    fpsFrameCount++;
    
    if (fpsAccumulator >= fpsUpdateInterval) {
        m_fps = static_cast<float>(fpsFrameCount) / fpsAccumulator;
        fpsAccumulator = 0.0f;
        fpsFrameCount = 0;
        
        // Log FPS periodically
        static uint32_t logCounter = 0;
        if (++logCounter % 60 == 0) { // Log every 60 seconds
            Logger::Info("VulkanRenderer", "FPS: " + std::to_string(static_cast<int>(m_fps)));
        }
    }
}

void VulkanRenderer::HandleVulkanError(VkResult result, const std::string& operation) {
    std::string errorString;
    
    switch (result) {
        case VK_SUCCESS:
            return; // No error
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            errorString = "VK_ERROR_OUT_OF_HOST_MEMORY";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            errorString = "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            errorString = "VK_ERROR_INITIALIZATION_FAILED";
            break;
        case VK_ERROR_DEVICE_LOST:
            errorString = "VK_ERROR_DEVICE_LOST";
            break;
        case VK_ERROR_SURFACE_LOST_KHR:
            errorString = "VK_ERROR_SURFACE_LOST_KHR";
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            errorString = "VK_ERROR_OUT_OF_DATE_KHR";
            break;
        case VK_SUBOPTIMAL_KHR:
            errorString = "VK_SUBOPTIMAL_KHR";
            break;
        default:
            errorString = "Unknown Vulkan error (" + std::to_string(result) + ")";
            break;
    }
    
    Logger::Error("VulkanRenderer", "Vulkan error during " + operation + ": " + errorString);
}

bool VulkanRenderer::InitializeShaders(Engine* owner) {
    Logger::Info("VulkanRenderer", "Initializing shaders");
    
    try {
        // Check if validation tools are available
        if (!ShaderValidator::AreValidationToolsAvailable()) {
            Logger::Warning("VulkanRenderer", "Shader validation tools not found. Skipping shader validation.");
        } else {
            Logger::Info("VulkanRenderer", "Shader validation tools are available.");
        }
        
        // Get base path from engine and construct shader paths
        std::filesystem::path basePath = owner->GetBasePath();
        std::filesystem::path vertexShaderPath = basePath / "Assets" / "Shaders" / "triangle.vert.spv";
        std::filesystem::path fragmentShaderPath = basePath / "Assets" / "Shaders" / "triangle.frag.spv";
        
        // Paths to GLSL source files for validation
        std::filesystem::path vertexGlslPath = basePath / "Assets" / "Shaders" / "triangle.vert";
        std::filesystem::path fragmentGlslPath = basePath / "Assets" / "Shaders" / "triangle.frag";
        
        Logger::Info("VulkanRenderer", "Base path: {}", basePath.string());
        Logger::Info("VulkanRenderer", "Vertex shader path: {}", vertexShaderPath.string());
        Logger::Info("VulkanRenderer", "Fragment shader path: {}", fragmentShaderPath.string());
        
        // Check if shader files exist
        if (!std::filesystem::exists(vertexShaderPath)) {
            Logger::Error("VulkanRenderer", "Vertex shader file not found: {}", vertexShaderPath.string());
            Logger::Error("VulkanRenderer", "Please ensure triangle.vert.spv exists in Assets/Shaders/");
            return false;
        }
        
        if (!std::filesystem::exists(fragmentShaderPath)) {
            Logger::Error("VulkanRenderer", "Fragment shader file not found: {}", fragmentShaderPath.string());
            Logger::Error("VulkanRenderer", "Please ensure triangle.frag.spv exists in Assets/Shaders/");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Shader files found, proceeding with initialization");
        
        // Validate SPIR-V shaders if validation tools are available
        if (ShaderValidator::AreValidationToolsAvailable()) {
            Logger::Info("VulkanRenderer", "Validating SPIR-V shaders...");
            
            if (!ShaderValidator::ValidateSPIRV(vertexShaderPath.string())) {
                Logger::Error("VulkanRenderer", "Vertex shader SPIR-V validation failed");
                return false;
            }
            
            if (!ShaderValidator::ValidateSPIRV(fragmentShaderPath.string())) {
                Logger::Error("VulkanRenderer", "Fragment shader SPIR-V validation failed");
                return false;
            }
            
            Logger::Info("VulkanRenderer", "SPIR-V shader validation successful");
        }
        
        // Vertex shader
        Logger::Info("VulkanRenderer", "Creating vertex shader...");
        m_vertexShader = std::make_unique<VulkanShader>();
        VulkanShader::Config vertexShaderConfig;
        vertexShaderConfig.filePath = vertexShaderPath.string();
        vertexShaderConfig.stage = VK_SHADER_STAGE_VERTEX_BIT;
        
        if (!m_vertexShader->Initialize(m_graphicsContext->GetDevice(), vertexShaderConfig)) {
            Logger::Error("VulkanRenderer", "Failed to initialize vertex shader: {}", m_vertexShader->GetLastError());
            Logger::Error("VulkanRenderer", "Vertex shader initialization failed - check shader compilation");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Vertex shader initialized successfully - module: {}", 
                    reinterpret_cast<uintptr_t>(m_vertexShader->GetModule()));
        
        // Fragment shader
        Logger::Info("VulkanRenderer", "Creating fragment shader...");
        m_fragmentShader = std::make_unique<VulkanShader>();
        VulkanShader::Config fragmentShaderConfig;
        fragmentShaderConfig.filePath = fragmentShaderPath.string();
        fragmentShaderConfig.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        if (!m_fragmentShader->Initialize(m_graphicsContext->GetDevice(), fragmentShaderConfig)) {
            Logger::Error("VulkanRenderer", "Failed to initialize fragment shader: {}", m_fragmentShader->GetLastError());
            Logger::Error("VulkanRenderer", "Fragment shader initialization failed - check shader compilation");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Fragment shader initialized successfully - module: {}", 
                    reinterpret_cast<uintptr_t>(m_fragmentShader->GetModule()));
        
        Logger::Info("VulkanRenderer", "All shaders initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Shader initialization failed with exception: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::InitializePipeline() {
    Logger::Info("VulkanRenderer", "Initializing graphics pipeline");
    
    try {
        m_pipeline = std::make_unique<VulkanPipeline>();
        VulkanPipeline::Config pipelineConfig;
        pipelineConfig.swapchain = m_graphicsContext->GetSwapchain();
        pipelineConfig.shaders = { m_vertexShader.get(), m_fragmentShader.get() };
        pipelineConfig.descriptorSetLayout = m_descriptorSetLayout;
        
        // Use full vertex input
        pipelineConfig.useMinimalVertexInput = false;
        
        // Try to initialize pipeline with timeout protection
        Logger::Info("VulkanRenderer", "Attempting pipeline initialization with full vertex input...");
        
        if (!m_pipeline->Initialize(m_graphicsContext->GetDevice(), pipelineConfig)) {
            Logger::Error("VulkanRenderer", "Failed to initialize pipeline: {}", m_pipeline->GetLastError());
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Graphics pipeline initialized successfully with full vertex input");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Pipeline initialization failed: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::CreateFallbackPipeline() {
    Logger::Info("VulkanRenderer", "Creating fallback graphics pipeline");
    
    try {
        // Create a minimal pipeline configuration
        m_pipeline = std::make_unique<VulkanPipeline>();
        VulkanPipeline::Config pipelineConfig;
        pipelineConfig.swapchain = m_graphicsContext->GetSwapchain();
        pipelineConfig.shaders = { m_vertexShader.get(), m_fragmentShader.get() };
        pipelineConfig.descriptorSetLayout = m_descriptorSetLayout;
        pipelineConfig.useMinimalVertexInput = true; // Use minimal vertex input for fallback
        
        // Use minimal vertex input state - no attributes for now
        Logger::Warning("VulkanRenderer", "Using minimal vertex input state for fallback");
        
        if (!m_pipeline->Initialize(m_graphicsContext->GetDevice(), pipelineConfig)) {
            Logger::Error("VulkanRenderer", "Fallback pipeline also failed: {}", m_pipeline->GetLastError());
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Fallback graphics pipeline created successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Fallback pipeline creation failed: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::InitializeVertexBuffer() {
    Logger::Info("VulkanRenderer", "Initializing vertex buffer");
    
    try {
        // Üçgen için vertex verileri
        std::vector<Vertex> vertices = {
            {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // Alt - kırmızı
            {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},   // Sağ üst - yeşil
            {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}   // Sol üst - mavi
        };
        
        // Vertex buffer oluştur
        m_vertexBuffer = std::make_unique<VulkanBuffer>();
        VulkanBuffer::Config bufferConfig;
        bufferConfig.size = sizeof(vertices[0]) * vertices.size();
        bufferConfig.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        if (!m_vertexBuffer->Initialize(m_graphicsContext->GetDevice(), bufferConfig)) {
            Logger::Error("VulkanRenderer", "Failed to initialize vertex buffer: {}", m_vertexBuffer->GetLastError());
            return false;
        }
        
        // Vertex verilerini buffer'a kopyala
        void* mappedData = m_vertexBuffer->Map();
        if (!mappedData) {
            Logger::Error("VulkanRenderer", "Failed to map vertex buffer");
            return false;
        }
        
        memcpy(mappedData, vertices.data(), bufferConfig.size);
        m_vertexBuffer->Unmap();
        
        Logger::Debug("VulkanRenderer", "Vertex buffer initialized with {} vertices", vertices.size());
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Vertex buffer initialization failed: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::CreateDescriptorSetLayout() {
    Logger::Info("VulkanRenderer", "Creating descriptor set layout");
    
    try {
        // UBO için binding tanımla
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;                                    // binding = 0
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // Uniform buffer
        uboLayoutBinding.descriptorCount = 1;                             // 1 descriptor
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;         // Vertex shader'da kullanılacak
        uboLayoutBinding.pImmutableSamplers = nullptr;                    // Sampler yok
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;
        
        VkResult result = vkCreateDescriptorSetLayout(m_graphicsContext->GetVkDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout);
        
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to create descriptor set layout: {}", static_cast<int32_t>(result));
            return false;
        }
        
        Logger::Debug("VulkanRenderer", "Descriptor set layout created successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Descriptor set layout creation failed: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::CreateDescriptorPool() {
    Logger::Info("VulkanRenderer", "Creating descriptor pool");
    
    try {
        // UBO descriptor'ları için pool size
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Serbest bırakma izni
        
        VkResult result = vkCreateDescriptorPool(m_graphicsContext->GetVkDevice(), &poolInfo, nullptr, &m_descriptorPool);
        
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to create descriptor pool: {}", static_cast<int32_t>(result));
            return false;
        }
        
        Logger::Debug("VulkanRenderer", "Descriptor pool created successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Descriptor pool creation failed: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::CreateDescriptorSets() {
    Logger::Info("VulkanRenderer", "Creating descriptor sets");
    
    try {
        // Descriptor set layout bilgilerini hazırla
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
        
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();
        
        // Descriptor set'leri ayır
        m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VkResult result = vkAllocateDescriptorSets(m_graphicsContext->GetVkDevice(), &allocInfo, m_descriptorSets.data());
        
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to allocate descriptor sets: {}", static_cast<int32_t>(result));
            return false;
        }
        
        Logger::Debug("VulkanRenderer", "Descriptor sets allocated successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Descriptor sets creation failed: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::CreateUniformBuffers() {
    Logger::Info("VulkanRenderer", "Creating uniform buffers");
    
    try {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        
        // Her frame için bir uniform buffer oluştur
        m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_uniformBuffers[i] = std::make_unique<VulkanBuffer>();
            
            VulkanBuffer::Config bufferConfig;
            bufferConfig.size = bufferSize;
            bufferConfig.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            
            if (!m_uniformBuffers[i]->Initialize(m_graphicsContext->GetDevice(), bufferConfig)) {
                Logger::Error("VulkanRenderer", "Failed to initialize uniform buffer {}: {}", i, m_uniformBuffers[i]->GetLastError());
                return false;
            }
            
            Logger::Debug("VulkanRenderer", "Uniform buffer {} created successfully", i);
        }
        
        Logger::Debug("VulkanRenderer", "All uniform buffers created successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Uniform buffers creation failed: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::UpdateDescriptorSets() {
    Logger::Info("VulkanRenderer", "Updating descriptor sets");
    
    try {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            // Buffer information
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffers[i]->GetBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);
            
            // Write descriptor set
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSets[i];
            descriptorWrite.dstBinding = 0;        // binding = 0
            descriptorWrite.dstArrayElement = 0;   // array element = 0
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr;   // Image info yok
            descriptorWrite.pTexelBufferView = nullptr; // Texel buffer view yok
            
            vkUpdateDescriptorSets(m_graphicsContext->GetVkDevice(), 1, &descriptorWrite, 0, nullptr);
        }
        
        Logger::Debug("VulkanRenderer", "Descriptor sets updated successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Descriptor sets update failed: {}", e.what());
        return false;
    }
}

void VulkanRenderer::DrawFrame() {
    if (!m_isInitialized || !m_graphicsContext || !m_commandBufferManager || !m_camera) {
        Logger::Error("VulkanRenderer", "Cannot draw frame - renderer not properly initialized");
        Logger::Error("VulkanRenderer", "  - Initialized: {}", m_isInitialized);
        Logger::Error("VulkanRenderer", "  - GraphicsContext: {}", m_graphicsContext != nullptr);
        Logger::Error("VulkanRenderer", "  - CommandBufferManager: {}", m_commandBufferManager != nullptr);
        Logger::Error("VulkanRenderer", "  - Camera: {}", m_camera != nullptr);
        return;
    }
    
    Logger::Debug("VulkanRenderer", "Starting DrawFrame - frame index: {}", m_currentFrame);
    
    try {
        // Synchronize frame index with command buffer manager
        SynchronizeFrameIndex();
        
        // Validate frame index bounds
        if (m_currentFrame >= MAX_FRAMES_IN_FLIGHT) {
            Logger::Error("VulkanRenderer", "Invalid frame index: {} (max: {}). Resetting to 0.", m_currentFrame, MAX_FRAMES_IN_FLIGHT - 1);
            m_currentFrame = 0;
        }
        
        Logger::Debug("VulkanRenderer", "Starting DrawFrame - frame: {}", m_currentFrame);
        
        // Update uniform buffer with current frame data
        UpdateUniformBuffer();
        
        // 1. Acquire next image from swapchain
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_graphicsContext->GetVkDevice(),
                                               m_graphicsContext->GetSwapchain()->GetSwapchain(),
                                               UINT64_MAX,
                                               m_commandBufferManager->GetImageAvailableSemaphore(m_currentFrame),
                                               VK_NULL_HANDLE, &imageIndex);
        
        Logger::Debug("VulkanRenderer", "vkAcquireNextImageKHR result: {}", static_cast<int32_t>(result));
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Pencere yeniden boyutlandırılmış, swapchain'i yeniden oluştur
            Logger::Info("VulkanRenderer", "Swapchain out of date, recreating...");
            m_graphicsContext->RecreateSwapchain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            HandleVulkanError(result, "vkAcquireNextImageKHR");
            return;
        }
        
        Logger::Debug("VulkanRenderer", "Acquired image index: {}", imageIndex);
        
        // 2. Record command buffer
        RecordCommandBuffer(imageIndex);
        
        // 3. Submit commands to GPU
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore waitSemaphores[] = {m_commandBufferManager->GetImageAvailableSemaphore(m_currentFrame)};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        
        VkCommandBuffer commandBuffer = m_commandBufferManager->GetCurrentCommandBuffer();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        VkSemaphore signalSemaphores[] = {m_commandBufferManager->GetRenderFinishedSemaphore(m_currentFrame)};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        Logger::Debug("VulkanRenderer", "Submitting command buffer to GPU");
        Logger::Debug("VulkanRenderer", "Current frame: {}, Image index: {}, Fence: {}",
                     m_currentFrame, imageIndex,
                     reinterpret_cast<uintptr_t>(m_commandBufferManager->GetInFlightFence(m_currentFrame)));
        
        // Validate synchronization objects before submission
        VkFence currentFence = m_commandBufferManager->GetInFlightFence(m_currentFrame);
        if (currentFence == VK_NULL_HANDLE) {
            Logger::Error("VulkanRenderer", "Invalid fence for frame {} - cannot submit command buffer", m_currentFrame);
            return;
        }
        
        result = vkQueueSubmit(m_graphicsContext->GetGraphicsQueue(), 1, &submitInfo, currentFence);
        
        Logger::Debug("VulkanRenderer", "vkQueueSubmit result: {}", static_cast<int32_t>(result));
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            Logger::Warning("VulkanRenderer", "VK_ERROR_OUT_OF_DATE_KHR/VK_SUBOPTIMAL_KHR detected during vkQueueSubmit - initiating swapchain recreation");
            
            // Critical: We must handle swapchain recreation during command submission
            // This requires careful coordination to avoid resource conflicts
            
            // 1. Wait for device to finish current operations
            Logger::Info("VulkanRenderer", "Waiting for device idle before swapchain recreation...");
            if (m_graphicsContext && m_graphicsContext->GetVkDevice() != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(m_graphicsContext->GetVkDevice());
            }
            
            // 2. Recreate swapchain
            Logger::Info("VulkanRenderer", "Recreating swapchain due to VK_ERROR_OUT_OF_DATE_KHR during vkQueueSubmit...");
            if (!m_graphicsContext->RecreateSwapchain()) {
                Logger::Error("VulkanRenderer", "Failed to recreate swapchain during VK_ERROR_OUT_OF_DATE_KHR recovery");
                return;
            }
            
            // 3. Reset frame index to ensure clean state
            Logger::Info("VulkanRenderer", "Resetting frame index after swapchain recreation...");
            m_currentFrame = 0;
            
            // 4. Skip this frame and let the next frame handle the new swapchain
            Logger::Info("VulkanRenderer", "Swapchain recreation completed - skipping current frame, next frame will use new swapchain");
            return;
        } else if (result != VK_SUCCESS) {
            HandleVulkanError(result, "vkQueueSubmit");
            return;
        }
        
        Logger::Debug("VulkanRenderer", "Command buffer submitted successfully");
        
        // 4. Present the frame
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        
        VkSwapchainKHR swapChains[] = {m_graphicsContext->GetSwapchain()->GetSwapchain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        
        Logger::Debug("VulkanRenderer", "Presenting frame");
        result = vkQueuePresentKHR(m_graphicsContext->GetPresentQueue(), &presentInfo);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            // Swapchain yeniden oluşturulması gerekiyor
            Logger::Info("VulkanRenderer", "Swapchain suboptimal or out of date detected during presentation - recreating...");
            if (!HandleSwapchainRecreation()) {
                Logger::Error("VulkanRenderer", "Failed to recreate swapchain during presentation error recovery");
                return;
            }
        } else if (result != VK_SUCCESS) {
            HandleVulkanError(result, "vkQueuePresentKHR");
            return;
        }
        
        Logger::Debug("VulkanRenderer", "Frame presented successfully");
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "DrawFrame failed: {}", e.what());
    }
}

void VulkanRenderer::RecordCommandBuffer(uint32_t imageIndex) {
    Logger::Debug("VulkanRenderer", "Recording command buffer for image index: {}", imageIndex);
    
    try {
        // Get current command buffer
        VkCommandBuffer commandBuffer = m_commandBufferManager->GetCurrentCommandBuffer();
        if (commandBuffer == VK_NULL_HANDLE) {
            Logger::Error("VulkanRenderer", "Invalid command buffer");
            return;
        }
        
        Logger::Debug("VulkanRenderer", "Using command buffer: {}", reinterpret_cast<uintptr_t>(commandBuffer));
        
        // Render pass başlat
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_graphicsContext->GetSwapchain()->GetRenderPass();
        renderPassInfo.framebuffer = m_graphicsContext->GetSwapchain()->GetFramebuffer(imageIndex);
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_graphicsContext->GetSwapchainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        Logger::Debug("VulkanRenderer", "Starting render pass with clear color: ({}, {}, {}, {})",
                     m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        Logger::Debug("VulkanRenderer", "Render pass begun");
        
        // Dinamik durumları ayarla
        Logger::Debug("VulkanRenderer", "Setting viewport: x={}, y={}, width={}, height={}",
                     m_currentViewport.x, m_currentViewport.y, m_currentViewport.width, m_currentViewport.height);
        vkCmdSetViewport(commandBuffer, 0, 1, &m_currentViewport);
        
        Logger::Debug("VulkanRenderer", "Setting scissor: offset=({},{}) extent={}x{}",
                     m_currentScissor.offset.x, m_currentScissor.offset.y,
                     m_currentScissor.extent.width, m_currentScissor.extent.height);
        vkCmdSetScissor(commandBuffer, 0, 1, &m_currentScissor);
        
        // Pipeline bağla
        VkPipeline pipeline = m_pipeline->GetPipeline();
        VkPipelineLayout layout = m_pipeline->GetLayout();
        Logger::Debug("VulkanRenderer", "Binding pipeline: {} with layout: {}",
                     reinterpret_cast<uintptr_t>(pipeline), reinterpret_cast<uintptr_t>(layout));
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        
        // Vertex buffer bağla
        VkBuffer vertexBuffer = m_vertexBuffer->GetBuffer();
        Logger::Debug("VulkanRenderer", "Binding vertex buffer: {}", reinterpret_cast<uintptr_t>(vertexBuffer));
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // Descriptor set bağla
        VkDescriptorSet descriptorSet = m_descriptorSets[m_currentFrame];
        Logger::Debug("VulkanRenderer", "Binding descriptor set: {} for frame {}",
                     reinterpret_cast<uintptr_t>(descriptorSet), m_currentFrame);
        VkDescriptorSet descriptorSets[] = {descriptorSet};
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, descriptorSets, 0, nullptr);
        
        // Üçgeni çiz - 3 vertex, 1 instance, 0 offset
        Logger::Debug("VulkanRenderer", "Drawing triangle: 3 vertices, 1 instance");
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        
        // Render pass'i bitir
        Logger::Debug("VulkanRenderer", "Ending render pass");
        vkCmdEndRenderPass(commandBuffer);
        
        Logger::Debug("VulkanRenderer", "Command buffer recording completed successfully");
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "RecordCommandBuffer failed: {}", e.what());
    }
}

void VulkanRenderer::ProcessRenderCommands(const std::vector<RenderCommand>& commands) {
    for (const auto& command : commands) {
        ExecuteRenderCommand(command);
    }
}

void VulkanRenderer::UpdateUniformBuffer() {
    try {
        // Calculate current time for animation
        float currentTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        float time = currentTime - m_startTime;
        
        // Create UBO data
        UniformBufferObject ubo{};
        
        // Model matrix: rotate around Y axis over time
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // View matrix: from camera
        ubo.view = m_camera->GetViewMatrix();
        
        // Projection matrix: from camera
        ubo.proj = m_camera->GetProjectionMatrix();
        
        // Map uniform buffer and copy data
        void* mappedData = m_uniformBuffers[m_currentFrame]->Map();
        if (!mappedData) {
            Logger::Error("VulkanRenderer", "Failed to map uniform buffer for frame {}", m_currentFrame);
            return;
        }
        
        memcpy(mappedData, &ubo, sizeof(ubo));
        m_uniformBuffers[m_currentFrame]->Unmap();
        
        Logger::Debug("VulkanRenderer", "Uniform buffer updated for frame {}", m_currentFrame);
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "UpdateUniformBuffer failed: {}", e.what());
    }
}

void VulkanRenderer::ExecuteRenderCommand(const RenderCommand& command) {
    // Implement command execution logic
    // This will handle different command types and translate them to Vulkan calls
    Logger::Debug("VulkanRenderer", "Executing render command type: {}", static_cast<int>(command.type));
    
    switch (command.type) {
        case RenderCommand::Type::DrawMesh:
            Logger::Debug("VulkanRenderer", "Executing DrawMesh command");
            DrawFrame();
            break;
        case RenderCommand::Type::SetViewport:
            Logger::Debug("VulkanRenderer", "Executing SetViewport command");
            break;
        case RenderCommand::Type::SetScissor:
            Logger::Debug("VulkanRenderer", "Executing SetScissor command");
            break;
        case RenderCommand::Type::BindPipeline:
            Logger::Debug("VulkanRenderer", "Executing BindPipeline command");
            break;
        case RenderCommand::Type::ClearColor:
            Logger::Debug("VulkanRenderer", "Executing ClearColor command");
            break;
        default:
            Logger::Warning("VulkanRenderer", "Unknown render command type: {}", static_cast<int>(command.type));
            break;
    }
}

bool VulkanRenderer::HandleSwapchainRecreation() {
    Logger::Info("VulkanRenderer", "Handling swapchain recreation...");
    
    try {
        // Wait for device to finish current operations
        if (m_graphicsContext && m_graphicsContext->GetVkDevice() != VK_NULL_HANDLE) {
            Logger::Debug("VulkanRenderer", "Waiting for device idle before swapchain recreation...");
            vkDeviceWaitIdle(m_graphicsContext->GetVkDevice());
        }
        
        // Recreate the swapchain
        Logger::Info("VulkanRenderer", "Recreating swapchain...");
        if (!m_graphicsContext->RecreateSwapchain()) {
            Logger::Error("VulkanRenderer", "Failed to recreate swapchain: {}", m_graphicsContext->GetLastError());
            return false;
        }
        
        // Recreate pipeline with new swapchain
        Logger::Info("VulkanRenderer", "Recreating graphics pipeline with new swapchain...");
        if (m_pipeline) {
            m_pipeline->Shutdown();
            m_pipeline.reset();
        }
        
        if (!InitializePipeline()) {
            Logger::Error("VulkanRenderer", "Failed to recreate pipeline after swapchain recreation");
            return false;
        }
        
        // Recreate framebuffers and related resources
        Logger::Info("VulkanRenderer", "Updating viewport and scissor for new swapchain extent...");
        VkExtent2D newExtent = m_graphicsContext->GetSwapchainExtent();
        SetViewport(0, 0, newExtent.width, newExtent.height);
        
        // Reset frame index to ensure proper synchronization
        Logger::Info("VulkanRenderer", "Resetting frame index after swapchain recreation...");
        m_currentFrame = 0;
        
        Logger::Info("VulkanRenderer", "Swapchain recreation completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Swapchain recreation failed with exception: {}", e.what());
        return false;
    }
}

void VulkanRenderer::SynchronizeFrameIndex() {
    uint32_t commandBufferFrame = m_commandBufferManager->GetCurrentFrameIndex();
    if (m_currentFrame != commandBufferFrame) {
        Logger::Warning("VulkanRenderer", "Frame index desynchronization detected - renderer: {}, command buffer: {}. Resyncing...",
                       m_currentFrame, commandBufferFrame);
        m_currentFrame = commandBufferFrame;
        
        // Ensure the frame index is within valid bounds
        if (m_currentFrame >= MAX_FRAMES_IN_FLIGHT) {
            Logger::Error("VulkanRenderer", "Command buffer manager returned invalid frame index: {} (max: {}). Resetting to 0.",
                         m_currentFrame, MAX_FRAMES_IN_FLIGHT - 1);
            m_currentFrame = 0;
        }
    }
}

} // namespace AstralEngine
