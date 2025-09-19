#include "VulkanRenderer.h"
#include "Core/Logger.h"
#include "Shaders/VulkanShader.h"
#include "Commands/VulkanPipeline.h"
#include "Buffers/VulkanMesh.h"
#include "Buffers/VulkanTexture.h"
#include "Subsystems/Renderer/RenderSubsystem.h"
#include "../RendererTypes.h"
#include "Subsystems/Asset/AssetData.h"
#include <vector>
#include <cstring>
#include <functional> // For std::hash

namespace AstralEngine {

// Define the structure for push constants used in the shadow pass
struct ShadowPushConstants {
    glm::mat4 lightSpaceMatrix;
    glm::mat4 modelMatrix;
};

// Vulkan'a özgü vertex input description helper yapısı
static struct VulkanVertexHelper {
    static VkPipelineVertexInputStateCreateInfo GetVertexInputState() {
        static auto bindingDescription = GetBindingDescriptions();
        static auto attributeDescriptions = GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        return vertexInputInfo;
    }

    static VkPipelineVertexInputStateCreateInfo GetVertexInputStateWithInstancing() {
        static auto bindingDescriptions = GetBindingDescriptionsWithInstancing();
        static auto attributeDescriptions = GetAttributeDescriptionsWithInstancing();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        return vertexInputInfo;
    }

private:
    static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;                                    // Binding index
        bindingDescription.stride = sizeof(Vertex);                       // Her vertex'in boyutu
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;      // Vertex başına veri
        
        bindingDescriptions.push_back(bindingDescription);
        return bindingDescriptions;
    }

    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

        // Position attribute (location = 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        // Normal attribute (location = 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        // Texture coordinate attribute (location = 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        // Tangent attribute (location = 3)
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, tangent);

        // Bitangent attribute (location = 4)
        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Vertex, bitangent);

        return attributeDescriptions;
    }

    static std::vector<VkVertexInputBindingDescription> GetBindingDescriptionsWithInstancing() {
        auto bindingDescriptions = GetBindingDescriptions();
        
        // Add binding for per-instance data (mat4)
        VkVertexInputBindingDescription instanceBinding{};
        instanceBinding.binding = 1;
        instanceBinding.stride = sizeof(glm::mat4);
        instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        bindingDescriptions.push_back(instanceBinding);
        
        return bindingDescriptions;
    }

    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptionsWithInstancing() {
        auto attributeDescriptions = GetAttributeDescriptions();
        
        // Add attributes for the mat4 columns
        for (int i = 0; i < 4; i++) {
            VkVertexInputAttributeDescription instanceAttrib{};
            instanceAttrib.binding = 1;
            instanceAttrib.location = 5 + i; // locations 0-4 are used by vertex data
            instanceAttrib.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4
            instanceAttrib.offset = sizeof(glm::vec4) * i;
            attributeDescriptions.push_back(instanceAttrib);
        }
        
        return attributeDescriptions;
    }
};

VulkanRenderer::VulkanRenderer() = default;
VulkanRenderer::~VulkanRenderer() = default;

bool VulkanRenderer::Initialize(GraphicsDevice* device, void* owner) {
    m_graphicsDevice = device;
    m_owner = static_cast<Engine*>(owner);
    m_renderSubsystem = m_owner->GetSubsystem<RenderSubsystem>();
    InitializeRenderingComponents();
    m_isInitialized = true;
    return true;
}

void VulkanRenderer::Shutdown() {
    ShutdownRenderingComponents();
    m_isInitialized = false;
}

bool VulkanRenderer::InitializeRenderingComponents() {
    CreateGBufferRenderPass();
    CreateLightingPass();
    CreateShadowPass();
    
    // In InitializeRenderingComponents()
    m_instanceBuffers.resize(m_graphicsDevice->GetMaxFramesInFlight());
    for (size_t i = 0; i < m_instanceBuffers.size(); ++i) {
        m_instanceBuffers[i] = std::make_unique<VulkanBuffer>();
        VulkanBuffer::Config config;
        config.size = INSTANCE_BUFFER_SIZE;
        config.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        config.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        m_instanceBuffers[i]->Initialize(m_graphicsDevice->GetVulkanDevice(), config);
    }
    
    return true;
}

void VulkanRenderer::ShutdownRenderingComponents() {
    vkDeviceWaitIdle(m_graphicsDevice->GetVulkanDevice()->GetDevice());
    vkDestroyRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), m_gBufferRenderPass, nullptr);
    vkDestroyRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), m_lightingRenderPass, nullptr);
    vkDestroyRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), m_shadowRenderPass, nullptr);
    vkDestroyDescriptorSetLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), m_lightingDescriptorSetLayout, nullptr);
    if (m_lightingPipeline) m_lightingPipeline->Shutdown();
    if (m_shadowPipeline) m_shadowPipeline->Shutdown();
    
    // Clear pipeline cache
    for (auto& pair : m_pipelineCache) {
        if (pair.second) {
            pair.second->Shutdown();
        }
    }
    m_pipelineCache.clear();
}

std::shared_ptr<VulkanPipeline> VulkanRenderer::GetOrCreatePipeline(const Material& material) {
    // Generate a hash from shader handles and relevant render state
    size_t materialHash = 0;
    std::hash<uint64_t> hasher;
    materialHash ^= hasher(material.GetVertexShaderHandle().GetID()) + 0x9e3779b9;
    materialHash ^= hasher(material.GetFragmentShaderHandle().GetID()) + 0x9e3779b9;
    // Add other state like blending, culling, etc., to the hash in the future.

    auto it = m_pipelineCache.find(materialHash);
    if (it != m_pipelineCache.end()) {
        return it->second;
    }

    // Pipeline not found, create a new one
    auto assetManager = m_renderSubsystem->GetAssetManager();
    auto vertexShader = assetManager->GetAsset<VulkanShader>(material.GetVertexShaderHandle());
    auto fragmentShader = assetManager->GetAsset<VulkanShader>(material.GetFragmentShaderHandle());

    if (!vertexShader || !fragmentShader) {
        Logger::Trace("VulkanRenderer", "Shaders for material '{}' not ready, cannot create pipeline.", material.GetName());
        return nullptr;
    }

    // Create pipeline configuration for G-Buffer pass
    VulkanPipeline::Config pipelineConfig;
    pipelineConfig.shaders = {vertexShader.get(), fragmentShader.get()};
    pipelineConfig.swapchain = m_graphicsDevice->GetSwapchain();
    pipelineConfig.extent = {m_graphicsDevice->GetSwapchain()->GetExtent().width, m_graphicsDevice->GetSwapchain()->GetExtent().height};
    pipelineConfig.descriptorSetLayout = material.GetDescriptorSetLayout();
    
    // Use the VulkanVertexHelper for vertex input state
    auto vertexInputState = VulkanVertexHelper::GetVertexInputStateWithInstancing();
    
    // Convert vertex input state to pipeline config format
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    
    // Extract binding descriptions
    if (vertexInputState.vertexBindingDescriptionCount > 0 && vertexInputState.pVertexBindingDescriptions) {
        for (uint32_t i = 0; i < vertexInputState.vertexBindingDescriptionCount; ++i) {
            bindingDescriptions.push_back(vertexInputState.pVertexBindingDescriptions[i]);
        }
    }
    
    // Extract attribute descriptions
    if (vertexInputState.vertexAttributeDescriptionCount > 0 && vertexInputState.pVertexAttributeDescriptions) {
        for (uint32_t i = 0; i < vertexInputState.vertexAttributeDescriptionCount; ++i) {
            attributeDescriptions.push_back(vertexInputState.pVertexAttributeDescriptions[i]);
        }
    }
    
    pipelineConfig.vertexBindingDescriptions = bindingDescriptions;
    pipelineConfig.vertexAttributeDescriptions = attributeDescriptions;
    pipelineConfig.useMinimalVertexInput = false;

    auto newPipeline = std::make_shared<VulkanPipeline>();
    if (!newPipeline->Initialize(m_graphicsDevice->GetVulkanDevice(), pipelineConfig)) {
        Logger::Error("VulkanRenderer", "Failed to create pipeline for material '{}'", material.GetName());
        return nullptr;
    }

    Logger::Info("VulkanRenderer", "Created and cached new pipeline for material '{}' (Hash: {})", material.GetName(), materialHash);
    m_pipelineCache[materialHash] = newPipeline;
    return newPipeline;
}

void VulkanRenderer::CreateShadowPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VulkanUtils::FindDepthFormat(m_graphicsDevice->GetVulkanDevice()->GetPhysicalDevice());
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    vkCreateRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &renderPassInfo, nullptr, &m_shadowRenderPass);

    VulkanPipeline::Config pipelineConfig{};
    pipelineConfig.device = m_graphicsDevice->GetVulkanDevice();
    pipelineConfig.renderPass = m_shadowRenderPass;
    pipelineConfig.vertexShaderPath = "Assets/Shaders/Materials/shadow.slang";
    pipelineConfig.fragmentShaderPath = "Assets/Shaders/Materials/shadow.slang";
    pipelineConfig.cullMode = VK_CULL_MODE_FRONT_BIT;
    pipelineConfig.depthBiasEnable = VK_TRUE;
    pipelineConfig.colorBlendAttachments = {};
    pipelineConfig.pushConstantRanges = {{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants)}};
    m_shadowPipeline = std::make_unique<VulkanPipeline>();
    m_shadowPipeline->Initialize(pipelineConfig);
}

void VulkanRenderer::RecordShadowPassCommands(VulkanFramebuffer* shadowFramebuffer, const glm::mat4& lightSpaceMatrix, const std::vector<ResolvedRenderItem>& renderItems) {
    VkCommandBuffer commandBuffer = m_graphicsDevice->GetCurrentCommandBuffer();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_shadowRenderPass;
    renderPassInfo.framebuffer = shadowFramebuffer->GetFramebuffer();
    renderPassInfo.renderArea.extent = {shadowFramebuffer->GetWidth(), shadowFramebuffer->GetHeight()};
    VkClearValue clearValue;
    clearValue.depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline->GetPipeline());
    vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);

    for (const auto& item : renderItems) {
        if (!item.mesh) continue;
        ShadowPushConstants pushData = {lightSpaceMatrix, item.transform};
        vkCmdPushConstants(commandBuffer, m_shadowPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &pushData);
        item.mesh->Bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, item.mesh->GetIndexCount(), 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);
}

// Other pass implementations (GBuffer, Lighting) are not shown for brevity, but are assumed to exist from previous steps.
void VulkanRenderer::RecordGBufferCommands(uint32_t frameIndex, VulkanFramebuffer* gBuffer, const std::map<MeshMaterialKey, std::vector<glm::mat4>>& renderQueue) {
    if (!m_graphicsDevice || !gBuffer || m_gBufferRenderPass == VK_NULL_HANDLE) {
        Logger::Error("VulkanRenderer", "Invalid parameters for G-Buffer pass");
        return;
    }

    VkCommandBuffer commandBuffer = m_graphicsDevice->GetCurrentCommandBuffer();
    
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_gBufferRenderPass;
    renderPassInfo.framebuffer = gBuffer->GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {gBuffer->GetWidth(), gBuffer->GetHeight()};
    
    std::array<VkClearValue, 4> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[3].depthStencil = {1.0f, 0};
    
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    auto materialManager = m_renderSubsystem->GetMaterialManager();
    auto meshManager = m_renderSubsystem->GetVulkanMeshManager();

    for (const auto& pair : renderQueue) {
        const MeshMaterialKey& key = pair.first;
        const auto& transforms = pair.second;

        if (transforms.empty()) {
            continue;
        }

        auto material = materialManager->GetOrCreateMaterial(key.materialHandle);
        auto mesh = meshManager->GetOrCreateMesh(key.modelHandle);

        if (!material || !mesh || !mesh->IsReady()) {
            continue;
        }
        
        // Get pipeline from the renderer's cache
        std::shared_ptr<VulkanPipeline> pipeline = GetOrCreatePipeline(*material);
        if (!pipeline) {
            continue; // Shaders not ready, skip for this frame
        }
        
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());
        
        VkDescriptorSet descriptorSet = material->GetDescriptorSet();
        if (descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->GetLayout(), 0, 1, &descriptorSet, 0, nullptr);
        }
        
        // Inside the loop in RecordGBufferCommands
        const auto& transforms = pair.second;
        uint32_t instanceCount = static_cast<uint32_t>(transforms.size());
        VkDeviceSize dataSize = sizeof(glm::mat4) * instanceCount;

        // Check for buffer overflow
        if (m_instanceBufferOffset + dataSize > INSTANCE_BUFFER_SIZE) {
            // Handle overflow: either flush and get a new buffer, or log an error
            Logger::Error("VulkanRenderer", "Instance buffer overflow!");
            continue;
        }

        // Copy data to the mapped buffer
        memcpy(static_cast<char*>(m_instanceBufferMapped) + m_instanceBufferOffset, transforms.data(), dataSize);

        mesh->Bind(commandBuffer); // Binds vertex buffer at binding 0

        // Bind the large instance buffer with the correct offset
        uint32_t currentFrameIndex = m_graphicsDevice->GetCurrentFrameIndex();
        VkBuffer instanceBuffers[] = { m_instanceBuffers[currentFrameIndex]->GetBuffer() };
        VkDeviceSize offsets[] = { m_instanceBufferOffset };
        vkCmdBindVertexBuffers(commandBuffer, 1, 1, instanceBuffers, offsets);

        vkCmdDrawIndexedInstanced(commandBuffer, mesh->GetIndexCount(), instanceCount, 0, 0, 0);

        // Advance the offset for the next draw call
        m_instanceBufferOffset += dataSize;
    }
    
    vkCmdEndRenderPass(commandBuffer);
}
void VulkanRenderer::RecordLightingCommands(uint32_t frameIndex, VulkanFramebuffer* sceneFramebuffer) {
    VkCommandBuffer commandBuffer = m_graphicsDevice->GetCurrentCommandBuffer();

    // Update descriptor set
    std::array<VkWriteDescriptorSet, 6> descriptorWrites{};

    VkDescriptorBufferInfo sceneUboInfo = m_renderSubsystem->GetSceneUBO()->GetDescriptorInfo();
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_lightingDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &sceneUboInfo;

    VkDescriptorImageInfo albedoInfo = m_renderSubsystem->GetAlbedoTexture()->GetDescriptorInfo();
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_lightingDescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &albedoInfo;

    VkDescriptorImageInfo normalInfo = m_renderSubsystem->GetNormalTexture()->GetDescriptorInfo();
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = m_lightingDescriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = &normalInfo;

    VkDescriptorImageInfo pbrInfo = m_renderSubsystem->GetPBRTexture()->GetDescriptorInfo();
    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = m_lightingDescriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pImageInfo = &pbrInfo;

    VkDescriptorImageInfo depthInfo = m_renderSubsystem->GetDepthTexture()->GetDescriptorInfo();
    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = m_lightingDescriptorSet;
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].dstArrayElement = 0;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pImageInfo = &depthInfo;

    VkDescriptorImageInfo shadowInfo = m_renderSubsystem->GetShadowMapTexture()->GetDescriptorInfo();
    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = m_lightingDescriptorSet;
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pImageInfo = &shadowInfo;

    vkUpdateDescriptorSets(m_graphicsDevice->GetVulkanDevice()->GetDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_lightingRenderPass;
    renderPassInfo.framebuffer = sceneFramebuffer->GetFramebuffer();
    renderPassInfo.renderArea.extent = { sceneFramebuffer->GetWidth(), sceneFramebuffer->GetHeight() };
    VkClearValue clearValue;
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind pipeline and descriptors
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline->GetPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline->GetLayout(), 0, 1, &m_lightingDescriptorSet, 0, nullptr);

    // Draw full-screen triangle
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);
}
void VulkanRenderer::CreateGBufferRenderPass() {
    // G-Buffer için attachment'ları tanımla
    // 1. Albedo (Renk) attachment
    VkAttachmentDescription albedoAttachment{};
    albedoAttachment.format = VK_FORMAT_R8G8B8A8_UNORM; // RGBA format
    albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    albedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    albedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 2. Normal (Normal vektörleri) attachment
    VkAttachmentDescription normalAttachment{};
    normalAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT; // Yüksek hassasiyetli normal vektörleri
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 3. PBR (Metallic, Roughness, AO) attachment
    VkAttachmentDescription pbrAttachment{};
    pbrAttachment.format = VK_FORMAT_R8G8B8A8_UNORM; // Metallic, Roughness, AO ve boş kanal
    pbrAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    pbrAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    pbrAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    pbrAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    pbrAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    pbrAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pbrAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 4. Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VulkanUtils::FindDepthFormat(m_graphicsDevice->GetVulkanDevice()->GetPhysicalDevice());
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    // Attachment referanslarını tanımla
    VkAttachmentReference albedoAttachmentRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference normalAttachmentRef{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference pbrAttachmentRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthAttachmentRef{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    // Subpass tanımla
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 3;
    VkAttachmentReference colorAttachments[] = {albedoAttachmentRef, normalAttachmentRef, pbrAttachmentRef};
    subpass.pColorAttachments = colorAttachments;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependency'leri tanımla
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Render pass oluştur
    std::array<VkAttachmentDescription, 4> attachments = {albedoAttachment, normalAttachment, pbrAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &renderPassInfo, nullptr, &m_gBufferRenderPass) != VK_SUCCESS) {
        Logger::Error("VulkanRenderer", "Failed to create G-Buffer render pass");
        return;
    }

    Logger::Info("VulkanRenderer", "G-Buffer render pass created successfully");
}
void VulkanRenderer::CreateLightingPass() {
    // Create Descriptor Set Layout
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    // Scene UBO
    bindings.push_back({0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    // G-Buffer Textures
    bindings.push_back({1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // Albedo
    bindings.push_back({2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // Normal
    bindings.push_back({3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // PBR
    bindings.push_back({4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // Depth
    bindings.push_back({5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // Shadow Map

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &layoutInfo, nullptr, &m_lightingDescriptorSetLayout) != VK_SUCCESS) {
        Logger::Error("VulkanRenderer", "Failed to create lighting descriptor set layout");
        return;
    }

    // Create Descriptor Set
    m_lightingDescriptorSet = m_graphicsDevice->GetVulkanDevice()->AllocateDescriptorSet(m_lightingDescriptorSetLayout);

    // Create Render Pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_graphicsDevice->GetSwapchain()->GetImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &renderPassInfo, nullptr, &m_lightingRenderPass) != VK_SUCCESS) {
        Logger::Error("VulkanRenderer", "Failed to create lighting render pass");
        return;
    }

    // Create Pipeline
    VulkanPipeline::Config pipelineConfig{};
    pipelineConfig.device = m_graphicsDevice->GetVulkanDevice();
    pipelineConfig.renderPass = m_lightingRenderPass;
    pipelineConfig.vertexShaderPath = "Assets/Shaders/Materials/deferred_lighting.slang";
    pipelineConfig.fragmentShaderPath = "Assets/Shaders/Materials/deferred_lighting.slang";
    pipelineConfig.cullMode = VK_CULL_MODE_NONE;
    pipelineConfig.depthTestEnable = VK_FALSE;
    pipelineConfig.depthWriteEnable = VK_FALSE;
    pipelineConfig.descriptorSetLayouts = {m_lightingDescriptorSetLayout};
    
    m_lightingPipeline = std::make_unique<VulkanPipeline>();
    m_lightingPipeline->Initialize(pipelineConfig);

    Logger::Info("VulkanRenderer", "Lighting pass created successfully");
}

void VulkanRenderer::ResetInstanceBuffer() {
    // At the beginning of the frame's rendering commands
    m_instanceBufferOffset = 0;
    uint32_t frameIndex = m_graphicsDevice->GetCurrentFrameIndex();
    m_instanceBufferMapped = m_instanceBuffers[frameIndex]->Map();
}


void VulkanRenderer::EndFrame() {
    // At the end of the frame's rendering commands
    uint32_t frameIndex = m_graphicsDevice->GetCurrentFrameIndex();
    m_instanceBuffers[frameIndex]->Unmap();
    m_instanceBufferMapped = nullptr;
}

}