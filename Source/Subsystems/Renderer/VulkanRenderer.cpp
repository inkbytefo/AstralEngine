#include "VulkanRenderer.h"
#include "Core/Logger.h"
#include "Shaders/VulkanShader.h"
#include "Commands/VulkanPipeline.h"
#include "Buffers/VulkanBuffer.h"
#include "Subsystems/Platform/PlatformSubsystem.h"
#include "Subsystems/Renderer/RenderSubsystem.h"
#include "Subsystems/Asset/AssetSubsystem.h"
#include "Subsystems/ECS/ECSSubsystem.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace AstralEngine {

VulkanRenderer::VulkanRenderer()
: m_frameTime(0.0f)
, m_isInitialized(false)
, m_isFrameStarted(false)
, m_lastError("") {
    
    // Set default configuration
    m_config = Config{};
    m_config.enableValidationLayers = true;
    
    // Initialize clear color to black
    m_clearColor[0] = 0.0f;  // R
    m_clearColor[1] = 0.0f;  // G  
    m_clearColor[2] = 0.0f;  // B
    m_clearColor[3] = 1.0f;  // A
    
    // TEMPORARY: Set red background for testing - triangle visibility debug
    m_clearColor[0] = 0.2f;  // R - Dark red background
    m_clearColor[1] = 0.0f;  // G
    m_clearColor[2] = 0.0f;  // B
    m_clearColor[3] = 1.0f;  // A
}

VulkanRenderer::~VulkanRenderer() {
    if (m_isInitialized) {
        Shutdown();
    }
}

bool VulkanRenderer::Initialize(GraphicsDevice* device, void* owner) {
    if (m_isInitialized) {
        Logger::Error("VulkanRenderer", "VulkanRenderer already initialized");
        return false;
    }
    
    try {
        Logger::Info("VulkanRenderer", "Initializing simplified VulkanRenderer...");
        
        m_graphicsDevice = device;
        m_owner = static_cast<Engine*>(owner);
        
        // Initialize rendering components
        if (!InitializeRenderingComponents()) {
            Logger::Error("VulkanRenderer", "Failed to initialize rendering components");
            return false;
        }
        
        m_isInitialized = true;
        Logger::Info("VulkanRenderer", "Simplified VulkanRenderer initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "VulkanRenderer initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanRenderer::Shutdown() {
    // IRenderer interface - actual shutdown done in OnShutdown
}



void VulkanRenderer::BeginFrame() {
    if (!m_isInitialized || m_isFrameStarted) {
        return;
    }
    
    m_isFrameStarted = true;
}

void VulkanRenderer::EndFrame() {
    if (!m_isInitialized || !m_isFrameStarted) {
        return;
    }
    
    m_isFrameStarted = false;
}

void VulkanRenderer::Present() {
    // Present logic is handled in DrawFrame for now
    // TODO: Implement proper present logic here
}

void VulkanRenderer::Submit(const RenderCommand& command) {
    // TODO: Implement command submission
    Logger::Debug("VulkanRenderer", "Submit command - type: {}", static_cast<int>(command.type));
}

void VulkanRenderer::SubmitCommands(const std::vector<RenderCommand>& commands) {
    // TODO: Implement command submission
    Logger::Debug("VulkanRenderer", "Submit {} commands", commands.size());
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
    Logger::Info("VulkanRenderer", "VulkanRenderer configuration updated");
}

bool VulkanRenderer::InitializeRenderingComponents() {
    try {
        Logger::Info("VulkanRenderer", "Initializing rendering components...");
        
        // Initialize shaders
        if (!InitializeShaders(m_owner)) {
            Logger::Error("VulkanRenderer", "Failed to initialize shaders");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Shaders initialized successfully");
        
        // Initialize pipeline - GraphicsDevice'dan descriptor set layout'ı al
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
        
        // Set start time for animation
        m_startTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        Logger::Info("VulkanRenderer", "Start time set for animation");
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Rendering component initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VulkanRenderer::ShutdownRenderingComponents() {
    try {
        Logger::Info("VulkanRenderer", "Shutting down rendering components...");
        
        // Wait for device to finish operations
        if (m_graphicsDevice && m_graphicsDevice->GetDevice() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_graphicsDevice->GetDevice());
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
        
        Logger::Info("VulkanRenderer", "Rendering components shutdown completed");
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Rendering component shutdown failed: " + std::string(e.what()));
    }
}

bool VulkanRenderer::InitializeShaders(Engine* owner) {
    Logger::Info("VulkanRenderer", "Initializing shaders using AssetManager");
    
    try {
        // Check if owner is valid
        if (!owner) {
            Logger::Error("VulkanRenderer", "Engine owner is null - cannot get AssetManager");
            return false;
        }
        
        // Get AssetSubsystem and AssetManager
        auto assetSubsystem = owner->GetSubsystem<AssetSubsystem>();
        if (!assetSubsystem) {
            Logger::Error("VulkanRenderer", "AssetSubsystem not found");
            return false;
        }
        
        auto assetManager = assetSubsystem->GetAssetManager();
        if (!assetManager) {
            Logger::Error("VulkanRenderer", "AssetManager not found");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "AssetManager obtained successfully, loading shaders");
        
        // Load shaders using AssetManager
        Logger::Info("VulkanRenderer", "Loading shaders using AssetManager...");
        
        // AssetManager'dan shader'ları yükle
        Logger::Info("VulkanRenderer", "Loading vertex shader 'triangle' from AssetManager...");
        auto vertexShader = assetManager->LoadShader("triangle", m_graphicsDevice->GetVulkanDevice());
        if (!vertexShader) {
            Logger::Error("VulkanRenderer", "Failed to load vertex shader from AssetManager");
            Logger::Error("VulkanRenderer", "This could indicate missing shader files or AssetManager initialization issues");
            return false;
        }
        Logger::Info("VulkanRenderer", "Vertex shader loaded successfully");
        
        // Fragment shader'ı cache'den al
        Logger::Info("VulkanRenderer", "Loading fragment shader 'triangle_fragment' from cache...");
        auto fragmentShader = assetManager->GetAssetFromCache<VulkanShader>("triangle_fragment");
        if (!fragmentShader) {
            Logger::Error("VulkanRenderer", "Failed to get fragment shader from cache");
            Logger::Error("VulkanRenderer", "This could indicate that the fragment shader was not properly cached");
            return false;
        }
        Logger::Info("VulkanRenderer", "Fragment shader loaded successfully from cache");
        
        // Shader'ların geçerliliğini kontrol et
        if (!vertexShader->IsInitialized() || !fragmentShader->IsInitialized()) {
            Logger::Error("VulkanRenderer", "One or more shaders are not initialized");
            if (!vertexShader->IsInitialized()) {
                Logger::Error("VulkanRenderer", "Vertex shader is not initialized");
            }
            if (!fragmentShader->IsInitialized()) {
                Logger::Error("VulkanRenderer", "Fragment shader is not initialized");
            }
            return false;
        }
        
        // AssetManager'dan gelen shader'ları doğrudan kullan
        // Not: Bu yaklaşımda shader'ların yaşam döngüsü AssetManager tarafından yönetilir
        m_vertexShader = vertexShader;
        m_fragmentShader = fragmentShader;
        
        Logger::Info("VulkanRenderer", "Both shaders loaded and validated successfully using AssetManager");
        
        Logger::Info("VulkanRenderer", "All shaders loaded successfully using AssetManager");
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
        
        // CRITICAL FIX: Eksik swapchain atamasını ekle
        pipelineConfig.swapchain = m_graphicsDevice->GetSwapchain();
        pipelineConfig.extent = m_graphicsDevice->GetSwapchain()->GetExtent();
        
        pipelineConfig.shaders = { m_vertexShader.get(), m_fragmentShader.get() };
        pipelineConfig.descriptorSetLayout = m_graphicsDevice->GetDescriptorSetLayout();
        pipelineConfig.useMinimalVertexInput = false;
        
        if (!m_pipeline->Initialize(m_graphicsDevice->GetVulkanDevice(), pipelineConfig)) {
            Logger::Error("VulkanRenderer", "Failed to initialize pipeline: {}", m_pipeline->GetLastError());
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Graphics pipeline initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Pipeline initialization failed: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::InitializeVertexBuffer() {
    Logger::Info("VulkanRenderer", "Initializing vertex buffer");
    
    try {
        // Triangle vertex data - make it larger and more visible
        std::vector<Vertex> vertices = {
            {{0.0f, -0.8f}, {1.0f, 1.0f, 1.0f}},  // Bottom - white
            {{0.8f, 0.8f}, {1.0f, 1.0f, 1.0f}},   // Right top - white  
            {{-0.8f, 0.8f}, {1.0f, 1.0f, 1.0f}}   // Left top - white
        };
        
        // Create vertex buffer
        m_vertexBuffer = std::make_unique<VulkanBuffer>();
        VulkanBuffer::Config bufferConfig;
        bufferConfig.size = sizeof(vertices[0]) * vertices.size();
        bufferConfig.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        if (!m_vertexBuffer->Initialize(m_graphicsDevice->GetVulkanDevice(), bufferConfig)) {
            Logger::Error("VulkanRenderer", "Failed to initialize vertex buffer: {}", m_vertexBuffer->GetLastError());
            return false;
        }
        
        // Copy vertex data to buffer
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

void VulkanRenderer::RecordCommands(uint32_t frameIndex) {
    if (!m_isInitialized || !m_graphicsDevice) {
        Logger::Error("VulkanRenderer", "Cannot record commands - renderer not properly initialized");
        return;
    }
    
    Logger::Debug("VulkanRenderer", "Starting command recording - frame index: {}", frameIndex);
    
    try {
        // Update uniform buffer with current frame data
        UpdateUniformBuffer(frameIndex);
        
        // GraphicsDevice'den mevcut frame bilgilerini al
        uint32_t imageIndex = m_graphicsDevice->GetCurrentImageIndex();
        
        // Record command buffer for this frame
        RecordCommandBuffer(imageIndex, frameIndex);
        
        Logger::Debug("VulkanRenderer", "Commands recorded successfully for frame {}", frameIndex);
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "RecordCommands failed: {}", e.what());
    }
}

void VulkanRenderer::RecordCommands(uint32_t frameIndex, const ECSSubsystem::RenderPacket& renderPacket) {
    if (!m_isInitialized || !m_graphicsDevice) {
        Logger::Error("VulkanRenderer", "Cannot record commands - renderer not properly initialized");
        return;
    }
    
    Logger::Debug("VulkanRenderer", "Starting command recording with ECS data - frame index: {}, items: {}", frameIndex, renderPacket.renderItems.size());
    
    try {
        // Update uniform buffer with current frame data
        UpdateUniformBuffer(frameIndex);
        
        // GraphicsDevice'den mevcut frame bilgilerini al
        uint32_t imageIndex = m_graphicsDevice->GetCurrentImageIndex();
        
        // Record command buffer for this frame with ECS data
        RecordCommandBufferWithECS(imageIndex, frameIndex, renderPacket);
        
        Logger::Debug("VulkanRenderer", "Commands recorded successfully for frame {} with {} ECS items", frameIndex, renderPacket.renderItems.size());
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "RecordCommands with ECS failed: {}", e.what());
    }
}

void VulkanRenderer::RecordCommandBuffer(uint32_t imageIndex, uint32_t frameIndex) {
    Logger::Debug("VulkanRenderer", "Recording command buffer for image index: {}", imageIndex);
    
    try {
        // Boundary check: imageIndex değerini doğrula
        if (!m_graphicsDevice || !m_graphicsDevice->GetSwapchain()) {
            Logger::Error("VulkanRenderer", "GraphicsDevice or swapchain is null");
            return;
        }
        
        if (imageIndex >= m_graphicsDevice->GetSwapchain()->GetImageCount()) {
            Logger::Error("VulkanRenderer", "Image index {} out of range (swapchain has {} images)", 
                        imageIndex, m_graphicsDevice->GetSwapchain()->GetImageCount());
            return;
        }
        
        // Boundary check: frameIndex değerini doğrula
        if (frameIndex >= m_graphicsDevice->GetConfig().maxFramesInFlight) {
            Logger::Error("VulkanRenderer", "Frame index {} out of range (max frames: {})", 
                        frameIndex, m_graphicsDevice->GetConfig().maxFramesInFlight);
            return;
        }
        
        // GraphicsDevice'den mevcut command buffer'ı al
        VkCommandBuffer commandBuffer = m_graphicsDevice->GetCurrentCommandBuffer();
        if (!commandBuffer) {
            Logger::Error("VulkanRenderer", "Failed to get command buffer from GraphicsDevice");
            return;
        }
        
        // Begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional
        
        VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to begin recording command buffer: {}", static_cast<int32_t>(result));
            return;
        }
        
        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_graphicsDevice->GetSwapchain()->GetRenderPass();
        renderPassInfo.framebuffer = m_graphicsDevice->GetSwapchain()->GetFramebuffer(imageIndex);
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_graphicsDevice->GetSwapchain()->GetExtent();
        
        // Set clear values (color + depth)
        VkClearValue clearValues[2] = {};
        clearValues[0].color = {{m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]}};  // Color clear
        clearValues[1].depthStencil = {1.0f, 0};  // Depth clear to 1.0 (farthest), stencil to 0
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.pClearValues = clearValues;
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        // Bind graphics pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());
        
        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_graphicsDevice->GetSwapchain()->GetExtent().width);
        viewport.height = static_cast<float>(m_graphicsDevice->GetSwapchain()->GetExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_graphicsDevice->GetSwapchain()->GetExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        // Bind vertex buffer
        VkBuffer vertexBuffers[] = {m_vertexBuffer->GetBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // Bind descriptor sets
        VkDescriptorSet currentDescriptorSet = m_graphicsDevice->GetCurrentDescriptorSet(frameIndex);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->GetLayout(),
            0,
            1,
            &currentDescriptorSet,
            0,
            nullptr
        );
        
        // Draw triangle
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        
        // End render pass
        vkCmdEndRenderPass(commandBuffer);
        
        // End command buffer recording
        result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to end recording command buffer: {}", static_cast<int32_t>(result));
            return;
        }
        
        Logger::Debug("VulkanRenderer", "Command buffer recorded successfully for image index: {}", imageIndex);
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "RecordCommandBuffer failed: {}", e.what());
    }
}

void VulkanRenderer::RecordCommandBufferWithECS(uint32_t imageIndex, uint32_t frameIndex, const ECSSubsystem::RenderPacket& renderPacket) {
    Logger::Debug("VulkanRenderer", "Recording command buffer with ECS data for image index: {}, items: {}", imageIndex, renderPacket.renderItems.size());
    
    try {
        // Boundary check: imageIndex değerini doğrula
        if (!m_graphicsDevice || !m_graphicsDevice->GetSwapchain()) {
            Logger::Error("VulkanRenderer", "GraphicsDevice or swapchain is null in ECS recording");
            return;
        }
        
        if (imageIndex >= m_graphicsDevice->GetSwapchain()->GetImageCount()) {
            Logger::Error("VulkanRenderer", "Image index {} out of range (swapchain has {} images) in ECS recording", 
                        imageIndex, m_graphicsDevice->GetSwapchain()->GetImageCount());
            return;
        }
        
        // Boundary check: frameIndex değerini doğrula
        if (frameIndex >= m_graphicsDevice->GetConfig().maxFramesInFlight) {
            Logger::Error("VulkanRenderer", "Frame index {} out of range (max frames: {}) in ECS recording", 
                        frameIndex, m_graphicsDevice->GetConfig().maxFramesInFlight);
            return;
        }
        
        // GraphicsDevice'den mevcut command buffer'ı al
        VkCommandBuffer commandBuffer = m_graphicsDevice->GetCurrentCommandBuffer();
        if (!commandBuffer) {
            Logger::Error("VulkanRenderer", "Failed to get command buffer from GraphicsDevice in ECS recording");
            return;
        }
        
        // Begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional
        
        VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to begin recording command buffer in ECS recording: {}", static_cast<int32_t>(result));
            return;
        }
        
        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_graphicsDevice->GetSwapchain()->GetRenderPass();
        renderPassInfo.framebuffer = m_graphicsDevice->GetSwapchain()->GetFramebuffer(imageIndex);
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_graphicsDevice->GetSwapchain()->GetExtent();
        
        // Set clear values (color + depth) - same as non-ECS version
        VkClearValue clearValues[2] = {};
        clearValues[0].color = {{m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]}};  // Color clear
        clearValues[1].depthStencil = {1.0f, 0};  // Depth clear to 1.0 (farthest), stencil to 0
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.pClearValues = clearValues;
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        // Bind graphics pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());
        
        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_graphicsDevice->GetSwapchain()->GetExtent().width);
        viewport.height = static_cast<float>(m_graphicsDevice->GetSwapchain()->GetExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_graphicsDevice->GetSwapchain()->GetExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        // Bind descriptor sets
        VkDescriptorSet currentDescriptorSet = m_graphicsDevice->GetCurrentDescriptorSet(frameIndex);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->GetLayout(),
            0,
            1,
            &currentDescriptorSet,
            0,
            nullptr
        );
        
        // Process ECS render items
        for (const auto& renderItem : renderPacket.renderItems) {
            if (renderItem.visible) {
                // For now, use the hardcoded vertex buffer but apply the transform from ECS
                // TODO: Implement proper dynamic vertex buffer creation based on modelPath
                
                // Update uniform buffer with ECS transform
                UpdateUniformBufferWithECS(frameIndex, renderItem.transform);
                
                // Bind vertex buffer (using hardcoded triangle for now)
                VkBuffer vertexBuffers[] = {m_vertexBuffer->GetBuffer()};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                
                // Draw triangle with ECS transform
                vkCmdDraw(commandBuffer, 3, 1, 0, 0);
                
                Logger::Debug("VulkanRenderer", "Drew ECS item with model path: {}", renderItem.modelPath);
            }
        }
        
        // End render pass
        vkCmdEndRenderPass(commandBuffer);
        
        // End command buffer recording
        result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to end recording command buffer in ECS recording: {}", static_cast<int32_t>(result));
            return;
        }
        
        Logger::Debug("VulkanRenderer", "ECS command buffer recorded successfully for image index: {}", imageIndex);
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "RecordCommandBufferWithECS failed: {}", e.what());
    }
}

void VulkanRenderer::UpdateUniformBufferWithECS(uint32_t frameIndex, const glm::mat4& ecsTransform) {
    try {
        // Boundary check: frameIndex değerini doğrula
        if (!m_graphicsDevice) {
            Logger::Error("VulkanRenderer", "GraphicsDevice is null - cannot update uniform buffer with ECS");
            return;
        }
        
        if (frameIndex >= m_graphicsDevice->GetConfig().maxFramesInFlight) {
            Logger::Error("VulkanRenderer", "Frame index {} out of range for uniform buffer update with ECS", frameIndex);
            return;
        }
        
        // Create UBO data with ECS transform
        UniformBufferObject ubo{};
        
        // Model matrix: use ECS transform instead of animation
        ubo.model = ecsTransform;
        
        // View ve projection matrislerini kameradan al
        if (m_camera) {
            ubo.view = m_camera->GetViewMatrix();
            ubo.proj = m_camera->GetProjectionMatrix();
            Logger::Debug("VulkanRenderer", "Using camera matrices for ECS uniform buffer");
            
            // Camera matrislerini doğrula
            if (glm::any(glm::isnan(ubo.view[0])) || glm::any(glm::isnan(ubo.proj[0]))) {
                Logger::Error("VulkanRenderer", "Camera matrices contain NaN values - using fallback");
                // Fallback matrisler kullan
                ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                ubo.proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
            }
        } else {
            Logger::Warning("VulkanRenderer", "No camera set, using fallback matrices");
            // Fallback view matrix (looking at origin from Z axis)
            ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            // Fallback projection matrix
            ubo.proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        }
        
        // GraphicsDevice'dan uniform buffer wrapper'ını al
        VulkanBuffer* uniformBuffer = m_graphicsDevice->GetCurrentUniformBufferWrapper(frameIndex);
        if (!uniformBuffer) {
            Logger::Error("VulkanRenderer", "Failed to get uniform buffer wrapper from GraphicsDevice for ECS frame {}", frameIndex);
            return;
        }
        
        // Buffer'ı map et, veriyi kopyala ve unmap et
        void* mappedData = uniformBuffer->Map();
        if (!mappedData) {
            Logger::Error("VulkanRenderer", "Failed to map uniform buffer for ECS frame {}", frameIndex);
            return;
        }
        memcpy(mappedData, &ubo, sizeof(ubo));
        uniformBuffer->Unmap();
        
        Logger::Debug("VulkanRenderer", "ECS uniform buffer updated for frame {}", frameIndex);
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "UpdateUniformBufferWithECS failed: {}", e.what());
    }
}

void VulkanRenderer::UpdateUniformBuffer(uint32_t frameIndex) {
    try {
        // Boundary check: frameIndex değerini doğrula
        if (!m_graphicsDevice) {
            Logger::Error("VulkanRenderer", "GraphicsDevice is null - cannot update uniform buffer");
            return;
        }
        
        if (frameIndex >= m_graphicsDevice->GetConfig().maxFramesInFlight) {
            Logger::Error("VulkanRenderer", "Frame index {} out of range for uniform buffer update", frameIndex);
            return;
        }
        
        // Calculate current time for animation
        float currentTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        float time = currentTime - m_startTime;
        
        // Create UBO data
        UniformBufferObject ubo{};
        
        // Model matrix: rotate around Y axis over time
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // View ve projection matrislerini kameradan al
        if (m_camera) {
            ubo.view = m_camera->GetViewMatrix();
            ubo.proj = m_camera->GetProjectionMatrix();
            Logger::Debug("VulkanRenderer", "Using camera matrices for uniform buffer");
            
            // Camera matrislerini doğrula
            if (glm::any(glm::isnan(ubo.view[0])) || glm::any(glm::isnan(ubo.proj[0]))) {
                Logger::Error("VulkanRenderer", "Camera matrices contain NaN values - using fallback");
                // Fallback matrisler kullan
                ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                ubo.proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
            }
        } else {
            Logger::Warning("VulkanRenderer", "No camera set, using fallback matrices");
            // Fallback view matrix (looking at origin from Z axis)
            ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            // Fallback projection matrix
            ubo.proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        }
        
        // GraphicsDevice'dan uniform buffer wrapper'ını al
        VulkanBuffer* uniformBuffer = m_graphicsDevice->GetCurrentUniformBufferWrapper(frameIndex);
        if (!uniformBuffer) {
            Logger::Error("VulkanRenderer", "Failed to get uniform buffer wrapper from GraphicsDevice for frame {}", frameIndex);
            return;
        }
        
        // Buffer'ı map et, veriyi kopyala ve unmap et
        void* mappedData = uniformBuffer->Map();
        if (!mappedData) {
            Logger::Error("VulkanRenderer", "Failed to map uniform buffer for frame {}", frameIndex);
            return;
        }
        memcpy(mappedData, &ubo, sizeof(ubo));
        uniformBuffer->Unmap();
        
        Logger::Debug("VulkanRenderer", "Uniform buffer updated for frame {}", frameIndex);
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "UpdateUniformBuffer failed: {}", e.what());
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
        default:
            errorString = "Unknown Vulkan error (" + std::to_string(result) + ")";
            break;
    }
    
    Logger::Error("VulkanRenderer", "Vulkan error during {}: {}", operation, errorString);
}

// Dynamic Rendering Implementasyonu
bool VulkanRenderer::InitializeDynamicRendering() {
    Logger::Info("VulkanRenderer", "Initializing dynamic rendering");
    
    try {
        // Vulkan 1.4+ desteğini kontrol et
        if (m_graphicsDevice && m_graphicsDevice->GetConfig().apiVersion >= VK_API_VERSION_1_4) {
            Logger::Info("VulkanRenderer", "Vulkan 1.4+ detected, using core dynamic rendering");
            return true;
        }
        
        // Vulkan 1.3 veya daha düşükse, extension desteğini kontrol et
        if (m_graphicsDevice && m_graphicsDevice->GetVulkanDevice()) {
            // Extension desteğini kontrol et
            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(m_graphicsDevice->GetPhysicalDevice(), nullptr, &extensionCount, nullptr);
            
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(m_graphicsDevice->GetPhysicalDevice(), nullptr, &extensionCount, availableExtensions.data());
            
            bool hasDynamicRendering = false;
            for (const auto& extension : availableExtensions) {
                if (strcmp(extension.extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
                    hasDynamicRendering = true;
                    break;
                }
            }
            
            if (hasDynamicRendering) {
                Logger::Info("VulkanRenderer", "VK_KHR_dynamic_rendering extension available");
                return true;
            } else {
                Logger::Warning("VulkanRenderer", "Dynamic rendering not supported, falling back to traditional render passes");
                return false;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Dynamic rendering initialization failed: {}", e.what());
        return false;
    }
}

void VulkanRenderer::BeginDynamicRendering(VkCommandBuffer /*commandBuffer*/, uint32_t imageIndex) {
    Logger::Debug("VulkanRenderer", "Beginning dynamic rendering for image index: {}", imageIndex);
    
    try {
        // Get swapchain info
        auto* swapchain = m_graphicsDevice->GetSwapchain();
        if (!swapchain) {
            Logger::Error("VulkanRenderer", "Swapchain is null in BeginDynamicRendering");
            return;
        }
        
        VkExtent2D extent = swapchain->GetExtent();
        
        // Get image view for the current swapchain image
        VkImageView imageView = swapchain->GetImageView(imageIndex);
        if (imageView == VK_NULL_HANDLE) {
            Logger::Error("VulkanRenderer", "Image view is null for image index: {}", imageIndex);
            return;
        }
        
        // Color attachment info
        VkRenderingAttachmentInfoKHR colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        colorAttachment.imageView = imageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.float32[0] = m_clearColor[0];
        colorAttachment.clearValue.color.float32[1] = m_clearColor[1];
        colorAttachment.clearValue.color.float32[2] = m_clearColor[2];
        colorAttachment.clearValue.color.float32[3] = m_clearColor[3];
        
        // Rendering info
        VkRenderingInfoKHR renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = extent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        
        // Begin dynamic rendering - TEMPORARILY DISABLED
        // vkCmdBeginRenderingKHR(commandBuffer, &renderingInfo);
        Logger::Warning("VulkanRenderer", "Dynamic rendering temporarily disabled - using traditional render passes");
        
        Logger::Debug("VulkanRenderer", "Dynamic rendering setup completed (but not executed)");
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "BeginDynamicRendering failed: {}", e.what());
    }
}

void VulkanRenderer::EndDynamicRendering(VkCommandBuffer /*commandBuffer*/) {
    Logger::Debug("VulkanRenderer", "Ending dynamic rendering");
    
    try {
        // End dynamic rendering - TEMPORARILY DISABLED
        // vkCmdEndRenderingKHR(commandBuffer);
        Logger::Warning("VulkanRenderer", "Dynamic rendering end temporarily disabled - using traditional render passes");
        Logger::Debug("VulkanRenderer", "Dynamic rendering end completed (but not executed)");
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "EndDynamicRendering failed: {}", e.what());
    }
}

void VulkanRenderer::RecordDynamicRenderingCommands(uint32_t frameIndex, uint32_t imageIndex) {
    Logger::Debug("VulkanRenderer", "Recording dynamic rendering commands for image index: {}", imageIndex);
    
    try {
        // Update uniform buffer with current frame data
        UpdateUniformBuffer(frameIndex);
        
        // Get command buffer from GraphicsDevice
        VkCommandBuffer commandBuffer = m_graphicsDevice->GetCurrentCommandBuffer();
        if (!commandBuffer) {
            Logger::Error("VulkanRenderer", "Failed to get command buffer from GraphicsDevice");
            return;
        }
        
        // Begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;
        
        VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to begin recording command buffer: {}", static_cast<int32_t>(result));
            return;
        }
        
        // Begin dynamic rendering
        BeginDynamicRendering(commandBuffer, imageIndex);
        
        // Bind graphics pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());
        
        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_graphicsDevice->GetSwapchain()->GetExtent().width);
        viewport.height = static_cast<float>(m_graphicsDevice->GetSwapchain()->GetExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_graphicsDevice->GetSwapchain()->GetExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        // Bind vertex buffer
        VkBuffer vertexBuffers[] = {m_vertexBuffer->GetBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // Bind descriptor sets
        VkDescriptorSet currentDescriptorSet = m_graphicsDevice->GetCurrentDescriptorSet(frameIndex);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->GetLayout(),
            0,
            1,
            &currentDescriptorSet,
            0,
            nullptr
        );
        
        // Draw triangle
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        
        // End dynamic rendering
        EndDynamicRendering(commandBuffer);
        
        // End command buffer recording
        result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS) {
            Logger::Error("VulkanRenderer", "Failed to end recording command buffer: {}", static_cast<int32_t>(result));
            return;
        }
        
        Logger::Debug("VulkanRenderer", "Dynamic rendering commands recorded successfully for image index: {}", imageIndex);
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "RecordDynamicRenderingCommands failed: {}", e.what());
    }
}

} // namespace AstralEngine
