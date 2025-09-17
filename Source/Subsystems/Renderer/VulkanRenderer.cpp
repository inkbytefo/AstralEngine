#include "VulkanRenderer.h"
#include "RendererTypes.h"
#include "Core/Logger.h"
#include "Shaders/VulkanShader.h"
#include "Commands/VulkanPipeline.h"
#include "Buffers/VulkanBuffer.h"
#include "Buffers/VulkanMesh.h"
#include "Buffers/VulkanTexture.h"
#include "Material/Material.h"
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
        
        // AssetManager'ı ayarla
        if (m_owner) {
            auto assetSubsystem = m_owner->GetSubsystem<AssetSubsystem>();
            if (assetSubsystem) {
                m_assetManager = assetSubsystem->GetAssetManager();
                if (!m_assetManager) {
                    Logger::Error("VulkanRenderer", "Failed to get AssetManager from AssetSubsystem");
                    return false;
                }
            } else {
                Logger::Error("VulkanRenderer", "AssetSubsystem not found");
                return false;
            }
        } else {
            Logger::Error("VulkanRenderer", "Engine owner is null");
            return false;
        }
        
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
    // Vertex buffer shutdown is handled in ShutdownRenderingComponents
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
        
        // Initialize pipeline - GraphicsDevice'dan descriptor set layout'ı al
        if (!InitializePipeline()) {
            Logger::Error("VulkanRenderer", "Failed to initialize pipeline");
            return false;
        }
        
        Logger::Info("VulkanRenderer", "Pipeline initialized successfully");
        
        // Create fallback texture once and cache it
        m_fallbackTexture = CreateFallbackTexture();
        if (!m_fallbackTexture) {
            Logger::Warning("VulkanRenderer", "Failed to create fallback texture during initialization");
        }
        
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
        
        // Shutdown pipeline
        if (m_pipeline) {
            m_pipeline->Shutdown();
            m_pipeline.reset();
        }
        
        // Shutdown fallback texture
        if (m_fallbackTexture) {
            m_fallbackTexture->Shutdown();
            m_fallbackTexture.reset();
        }
        
        Logger::Info("VulkanRenderer", "Rendering components shutdown completed");
    }
    catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "Rendering component shutdown failed: " + std::string(e.what()));
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
        
        // Note: Shader'lar artık materyallerden dinamik olarak yüklenecek
        // Pipeline konfigürasyonu boş shader listesi ile başlatılacak
        pipelineConfig.shaders = {}; // Boş olarak başlat, materyallerden doldurulacak
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



void VulkanRenderer::RecordCommands(uint32_t frameIndex) {
    if (!m_isInitialized || !m_graphicsDevice) {
        Logger::Error("VulkanRenderer", "Cannot record commands - renderer not properly initialized");
        return;
    }
    
    Logger::Debug("VulkanRenderer", "Starting command recording - frame index: {}", frameIndex);
    
    try {
        // Update uniform buffer with current frame data
        UpdateUniformBuffer(frameIndex);
        
        // Note: Non-ECS command recording is no longer supported
        // Renderer now only works with ECS data through RecordCommandBufferWithECS
        Logger::Warning("VulkanRenderer", "Non-ECS command recording called - this is no longer supported");
        
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
        uint32_t imageIndex = m_graphicsDevice->GetFrameManager()->GetCurrentImageIndex();
        
        // Record command buffer for this frame with ECS data
        RecordCommandBufferWithECS(imageIndex, frameIndex, renderPacket);
        
        Logger::Debug("VulkanRenderer", "Commands recorded successfully for frame {} with {} ECS items", frameIndex, renderPacket.renderItems.size());
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "RecordCommands with ECS failed: {}", e.what());
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
        
        // Set viewport and scissor (tüm render item'lar için aynı)
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
        
        // Process ECS render items with material-driven rendering
        VkPipeline lastBoundPipeline = VK_NULL_HANDLE;
        VkPipelineLayout lastBoundPipelineLayout = VK_NULL_HANDLE;
        
        for (const auto& renderItem : renderPacket.renderItems) {
            if (!renderItem.visible) {
                continue;
            }
            
            // AssetHandle'ı al (önce AssetHandle, sonra legacy string path)
            AssetHandle modelHandle = renderItem.modelHandle.IsValid() ? renderItem.modelHandle : 
                                     (!renderItem.modelPath.empty() ? AssetHandle(renderItem.modelPath, AssetHandle::Type::Model) : AssetHandle());
            
            if (!modelHandle.IsValid()) {
                Logger::Warning("VulkanRenderer", "Render item has invalid model handle");
                continue;
            }
            
            // Asset durumunu kontrol et
            AssetLoadState modelState = m_assetManager ? m_assetManager->GetAssetState(modelHandle) : AssetLoadState::NotLoaded;
            
            if (modelState == AssetLoadState::Loaded) {
                // Model yüklüyse render et
                auto model = m_assetManager->GetAsset<Model>(modelHandle);
                if (!model) {
                    Logger::Error("VulkanRenderer", "Model marked as loaded but not found in cache: {}", modelHandle.GetID());
                    continue;
                }
                
                // Materyal handle'ını al
                AssetHandle materialHandle = renderItem.materialHandle.IsValid() ? renderItem.materialHandle : 
                                          (!renderItem.materialPath.empty() ? AssetHandle(renderItem.materialPath, AssetHandle::Type::Material) : AssetHandle());
                
                // Materyali al veya varsayılan materyal kullan
                std::shared_ptr<Material> material = nullptr;
                if (materialHandle.IsValid()) {
                    AssetLoadState materialState = m_assetManager->GetAssetState(materialHandle);
                    if (materialState == AssetLoadState::Loaded) {
                        material = m_assetManager->GetAsset<Material>(materialHandle);
                    }
                }
                
                // Eğer materyal yoksa, varsayılan PBR materyalini kullan
                if (!material) {
                    Logger::Warning("VulkanRenderer", "No valid material for render item, using default PBR material");
                    // TODO: MaterialManager'dan varsayılan materyali al
                    // material = m_materialManager->GetDefaultPBRMaterial();
                    continue; // Geçici olarak atla
                }
                
                // Materyalin pipeline'ını ve pipeline layout'ını al
                VkPipeline materialPipeline = material->GetPipeline();
                VkPipelineLayout materialPipelineLayout = material->GetPipelineLayout();
                
                if (materialPipeline == VK_NULL_HANDLE || materialPipelineLayout == VK_NULL_HANDLE) {
                    Logger::Error("VulkanRenderer", "Material has invalid pipeline or pipeline layout");
                    continue;
                }
                
                // Pipeline değiştiyse yeni pipeline'ı bind et
                if (materialPipeline != lastBoundPipeline) {
                    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, materialPipeline);
                    lastBoundPipeline = materialPipeline;
                    lastBoundPipelineLayout = materialPipelineLayout;
                    Logger::Debug("VulkanRenderer", "Bound new pipeline for material: {}", material->GetName());
                }
                
                // Materyalin descriptor set'ini al
                VkDescriptorSet materialDescriptorSet = material->GetDescriptorSet();
                if (materialDescriptorSet != VK_NULL_HANDLE) {
                    // Materyalin descriptor set'ini bind et
                    vkCmdBindDescriptorSets(
                        commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        materialPipelineLayout,
                        0,
                        1,
                        &materialDescriptorSet,
                        0,
                        nullptr
                    );
                }
                
                // Modelin içindeki her bir VulkanMesh için render et
                const auto& meshes = model->GetMeshes();
                for (const auto& mesh : meshes) {
                    if (!mesh) {
                        continue;
                    }
                    
                    // Materyalin uniform buffer'ını güncelle
                    material->UpdateUniformBuffer(frameIndex);
                    
                    // Mesh'in vertex ve index buffer'larını bind et
                    mesh->Bind(commandBuffer);
                    
                    // Draw using indexed drawing with the mesh's index count
                    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh->GetIndexCount()), 1, 0, 0, 0);
                    
                    Logger::Debug("VulkanRenderer", "Drew ECS item with model: {}, material: {}, index count: {}", 
                                 modelHandle.GetID(), material->GetName(), mesh->GetIndexCount());
                }
                
            } else if (modelState == AssetLoadState::Loading) {
                // Model yükleniyorsa placeholder render et
                Logger::Debug("VulkanRenderer", "Model still loading: {}, rendering placeholder", modelHandle.GetID());
                RenderPlaceholder(commandBuffer, frameIndex, renderItem.transform);
            } else {
                // Model yüklenmediyse veya başarısız olduysa
                Logger::Warning("VulkanRenderer", "Model not loaded (state: {}): {}", 
                               static_cast<int>(modelState), modelHandle.GetID());
                RenderPlaceholder(commandBuffer, frameIndex, renderItem.transform);
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

void VulkanRenderer::UpdateDescriptors(uint32_t frameIndex, const std::shared_ptr<VulkanTexture>& texture) {
    try {
        // Boundary check: frameIndex değerini doğrula
        if (!m_graphicsDevice) {
            Logger::Error("VulkanRenderer", "GraphicsDevice is null - cannot update descriptors");
            return;
        }
        
        if (frameIndex >= m_graphicsDevice->GetConfig().maxFramesInFlight) {
            Logger::Error("VulkanRenderer", "Frame index {} out of range for descriptor update", frameIndex);
            return;
        }
        
        if (!texture || !texture->IsInitialized()) {
            Logger::Error("VulkanRenderer", "Texture is null or not initialized - cannot update descriptors");
            return;
        }
        
        // Get current descriptor set
        VkDescriptorSet descriptorSet = m_graphicsDevice->GetCurrentDescriptorSet(frameIndex);
        if (descriptorSet == VK_NULL_HANDLE) {
            Logger::Error("VulkanRenderer", "Failed to get descriptor set for frame {}", frameIndex);
            return;
        }
        
        // Get uniform buffer
        VulkanBuffer* uniformBuffer = m_graphicsDevice->GetCurrentUniformBufferWrapper(frameIndex);
        if (!uniformBuffer) {
            Logger::Error("VulkanRenderer", "Failed to get uniform buffer for frame {}", frameIndex);
            return;
        }
        
        // Prepare descriptor writes
        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        
        // UBO descriptor write
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer->GetBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0; // UBO binding
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pTexelBufferView = nullptr;
        
        // Texture sampler descriptor write
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture->GetImageView();
        imageInfo.sampler = texture->GetSampler();
        
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1; // Texture sampler binding
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pImageInfo = &imageInfo;
        descriptorWrites[1].pTexelBufferView = nullptr;
        
        // Update descriptor sets
        vkUpdateDescriptorSets(m_graphicsDevice->GetDevice(), 
                              static_cast<uint32_t>(descriptorWrites.size()), 
                              descriptorWrites.data(), 0, nullptr);
        
        Logger::Debug("VulkanRenderer", "Descriptors updated successfully for frame {} with texture", frameIndex);
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "UpdateDescriptors failed: {}", e.what());
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
            if (glm::isnan(ubo.view[0].x) || glm::isnan(ubo.view[0].y) || glm::isnan(ubo.view[0].z) || glm::isnan(ubo.view[0].w) ||
                glm::isnan(ubo.proj[0].x) || glm::isnan(ubo.proj[0].y) || glm::isnan(ubo.proj[0].z) || glm::isnan(ubo.proj[0].w)) {
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
        
        // Uniform Buffer Object oluştur
        UniformBufferObject ubo{};
        
        // Model matrix'i birim matris olarak ayarla.
        // Gerçek transform ECS'den gelen veri ile UpdateUniformBufferWithECS içinde ayarlanacak.
        ubo.model = glm::mat4(1.0f);
        
        // View ve projection matrislerini kameradan al
        if (m_camera) {
            ubo.view = m_camera->GetViewMatrix();
            ubo.proj = m_camera->GetProjectionMatrix();
            Logger::Debug("VulkanRenderer", "Using camera matrices for uniform buffer");
            
            // Camera matrislerini doğrula
            if (glm::isnan(ubo.view[0].x) || glm::isnan(ubo.view[0].y) || glm::isnan(ubo.view[0].z) || glm::isnan(ubo.view[0].w) ||
                glm::isnan(ubo.proj[0].x) || glm::isnan(ubo.proj[0].y) || glm::isnan(ubo.proj[0].z) || glm::isnan(ubo.proj[0].w)) {
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

std::shared_ptr<VulkanTexture> VulkanRenderer::CreateFallbackTexture() {
    Logger::Info("VulkanRenderer", "Creating fallback white texture");
    
    try {
        if (!m_graphicsDevice || !m_graphicsDevice->GetVulkanDevice()) {
            Logger::Error("VulkanRenderer", "Cannot create fallback texture - VulkanDevice is null");
            return nullptr;
        }
        
        auto texture = std::make_shared<VulkanTexture>();
        
        // Create a 1x1 white texture
        std::vector<uint32_t> whitePixel = {0xFFFFFFFF}; // RGBA white
        
        if (!texture->InitializeFromData(m_graphicsDevice->GetVulkanDevice(), 
                                       whitePixel.data(), 1, 1, VK_FORMAT_R8G8B8A8_UNORM)) {
            Logger::Error("VulkanRenderer", "Failed to create fallback texture: {}", texture->GetLastError());
            return nullptr;
        }
        
        Logger::Info("VulkanRenderer", "Fallback white texture created successfully");
        return texture;
        
    } catch (const std::exception& e) {
        Logger::Error("VulkanRenderer", "CreateFallbackTexture failed: {}", e.what());
        return nullptr;
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
        
        // Note: Vertex buffer binding removed - now handled by ECS meshes
        
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

void VulkanRenderer::RenderModelWithAssetHandles(VkCommandBuffer commandBuffer, uint32_t frameIndex, 
                                               const ECSSubsystem::RenderPacket::RenderItem& renderItem, 
                                               const std::shared_ptr<Model>& model) {
    if (!model || !m_assetManager) {
        return;
    }
    
    // Modelin içindeki her bir VulkanMesh için render et
    const auto& meshes = model->GetMeshes();
    for (const auto& mesh : meshes) {
        if (!mesh) {
            continue;
        }
        
        // Texture handle'ını al - sadece AssetHandle kullan (yeni sistem)
        AssetHandle textureHandle = renderItem.textureHandle;
        
        // Texture durumunu kontrol et - sadece Loaded durumunda render et
        std::shared_ptr<VulkanTexture> texture = nullptr;
        if (textureHandle.IsValid()) {
            AssetLoadState textureState = m_assetManager->GetAssetState(textureHandle);
            
            if (textureState == AssetLoadState::Loaded) {
                texture = m_assetManager->GetAsset<VulkanTexture>(textureHandle);
                Logger::Debug("VulkanRenderer", "Using loaded texture: {}", textureHandle.GetID());
            } else {
                Logger::Warning("VulkanRenderer", "Texture not ready for rendering (state: {}): {}", 
                               static_cast<int>(textureState), textureHandle.GetID());
                texture = CreateFallbackTexture();
            }
        } else {
            // Geçerli texture handle'ı yoksa fallback kullan
            Logger::Debug("VulkanRenderer", "No valid texture handle, using fallback");
            texture = CreateFallbackTexture();
        }
        
        // Update descriptors with the texture
        if (texture && texture->IsInitialized()) {
            UpdateDescriptors(frameIndex, texture);
        }
        
        // Update uniform buffer with ECS transform
        UpdateUniformBufferWithECS(frameIndex, renderItem.transform);
        
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
        
        // Bind the mesh's vertex and index buffers
        mesh->Bind(commandBuffer);
        
        // Draw using indexed drawing with the mesh's index count
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh->GetIndexCount()), 1, 0, 0, 0);
        
        Logger::Debug("VulkanRenderer", "Drew ECS item with model ID: {}, index count: {}, texture: {}", 
                     renderItem.modelHandle.GetID(), mesh->GetIndexCount(), texture ? "loaded" : "fallback");
    }
}

void VulkanRenderer::RenderPlaceholder(VkCommandBuffer commandBuffer, uint32_t frameIndex, const glm::mat4& transform) {
    // Placeholder rendering - basit bir küp veya üçgen render et
    
    // Update uniform buffer with ECS transform
    UpdateUniformBufferWithECS(frameIndex, transform);
    
    // Use cached fallback texture instead of creating new one every frame
    if (m_fallbackTexture && m_fallbackTexture->IsInitialized()) {
        UpdateDescriptors(frameIndex, m_fallbackTexture);
        Logger::Debug("VulkanRenderer", "Using cached fallback texture for placeholder rendering");
    } else {
        Logger::Warning("VulkanRenderer", "Fallback texture not available, skipping texture binding");
    }
    
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
    
    // Placeholder olarak basit bir üçgen draw et
    // Not: Gerçek implementasyonda burada bir placeholder mesh kullanılmalı
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    
    Logger::Debug("VulkanRenderer", "Rendered placeholder for asset");
}

} // namespace AstralEngine
