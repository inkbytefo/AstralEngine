#include "VulkanRenderer.h"
#include "Core/Logger.h"
#include "Shaders/VulkanShader.h"
#include "Commands/VulkanPipeline.h"
#include "Buffers/VulkanMesh.h"
#include "Buffers/VulkanTexture.h"
#include "Subsystems/Renderer/RenderSubsystem.h"
#include "VulkanBindlessSystem.h"
#include "GraphicsDevice.h"
#include <array>

namespace AstralEngine {

VulkanRenderer::VulkanRenderer() = default;
VulkanRenderer::~VulkanRenderer() = default;

bool VulkanRenderer::Initialize(GraphicsDevice* device, void* owner) {
    m_graphicsDevice = device;
    m_owner = static_cast<Engine*>(owner);
    m_renderSubsystem = m_owner->GetSubsystem<RenderSubsystem>();
    
    // Create global descriptor set layout for bindless
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1000; // Large enough for bindless
    binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.bindingCount = 1;
    bindingFlags.pBindingFlags = &flags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &bindingFlags;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    vkCreateDescriptorSetLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &layoutInfo, nullptr, &m_globalDescriptorSetLayout);

    // Create global pipeline layout with push constants
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    std::array<VkDescriptorSetLayout, 2> layouts = {
        m_globalDescriptorSetLayout,
        m_graphicsDevice->GetBindlessSystem()->GetLayout()
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    vkCreatePipelineLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &pipelineLayoutInfo, nullptr, &m_globalPipelineLayout);
    
    // Initialize instance buffers
    m_instanceBuffers.resize(m_graphicsDevice->GetMaxFramesInFlight());
    for (size_t i = 0; i < m_instanceBuffers.size(); ++i) {
        m_instanceBuffers[i] = std::make_unique<VulkanBuffer>();
        VulkanBuffer::Config config;
        config.size = INSTANCE_BUFFER_SIZE;
        config.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        config.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        m_instanceBuffers[i]->Initialize(m_graphicsDevice->GetVulkanDevice(), config);
    }
    
    m_isInitialized = true;
    Logger::Info("VulkanRenderer", "Modern VulkanRenderer (2025) initialized");
    return true;
}

void VulkanRenderer::Shutdown() {
    vkDeviceWaitIdle(m_graphicsDevice->GetVulkanDevice()->GetDevice());
    
    for (auto& pair : m_pipelineCache) if (pair.second) pair.second->Shutdown();
    for (auto& pair : m_pipelineLayoutCache) vkDestroyPipelineLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), pair.second, nullptr);
    for (auto& pair : m_descriptorSetLayoutCache) vkDestroyDescriptorSetLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), pair.second, nullptr);
    
    m_pipelineCache.clear();
    m_pipelineLayoutCache.clear();
    m_descriptorSetLayoutCache.clear();
    m_isInitialized = false;
}

void VulkanRenderer::RecordShadowPassCommands(VulkanTexture* depthTarget, const glm::mat4& lightSpaceMatrix, const std::vector<ResolvedRenderItem>& renderItems) {
    VkCommandBuffer cmd = m_graphicsDevice->GetCurrentCommandBuffer();

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthTarget->GetImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {depthTarget->GetWidth(), depthTarget->GetHeight()}};
    renderingInfo.layerCount = 1;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);
    
    // Render items...
    // (Simplified for brevity, would use GetOrCreatePipeline with shadow flag)
    
    vkCmdEndRendering(cmd);
}

void VulkanRenderer::RecordGBufferCommands(uint32_t frameIndex, const std::vector<VulkanTexture*>& colorTargets, VulkanTexture* depthTarget, const std::map<MeshMaterialKey, std::vector<glm::mat4>>& renderQueue) {
    VkCommandBuffer cmd = m_graphicsDevice->GetCurrentCommandBuffer();

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    for (auto* target : colorTargets) {
        VkRenderingAttachmentInfo att{};
        att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        att.imageView = target->GetImageView();
        att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att.clearValue.color = {{0, 0, 0, 1}};
        colorAttachments.push_back(att);
    }

    VkRenderingAttachmentInfo depthAtt{};
    depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAtt.imageView = depthTarget->GetImageView();
    depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAtt.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {depthTarget->GetWidth(), depthTarget->GetHeight()}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = &depthAtt;

    vkCmdBeginRendering(cmd, &renderingInfo);

    auto bindlessSet = m_graphicsDevice->GetBindlessSystem()->GetDescriptorSet();

    for (const auto& [key, transforms] : renderQueue) {
        auto material = m_renderSubsystem->GetMaterialManager()->GetOrCreateMaterial(key.materialHandle);
        auto mesh = m_renderSubsystem->GetVulkanMeshManager()->GetOrCreateMesh(key.modelHandle);
        if (!material || !mesh || !mesh->IsReady()) continue;

        auto pipeline = GetOrCreatePipeline(*material);
        if (!pipeline) continue;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());
        
        // Bind globals: Bindless set at set 1
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(), 1, 1, &bindlessSet, 0, nullptr);
        
        // Push constants
        const auto& props = material->GetProperties();
        for (const auto& transform : transforms) {
            PushConstants pc;
            pc.model = transform;
            pc.baseColorFactor = props.baseColor;
            pc.metallicFactor = props.metallic;
            pc.roughnessFactor = props.roughness;
            pc.aoFactor = props.ao;
            pc.emissiveFactor = props.emissiveColor * props.emissiveIntensity;
            
            pc.albedoIndex = material->GetAlbedoIndex();
            pc.normalIndex = material->GetNormalIndex();
            pc.metallicIndex = material->GetMetallicIndex();
            pc.roughnessIndex = material->GetRoughnessIndex();
            pc.aoIndex = material->GetAOIndex();
            pc.emissiveIndex = material->GetEmissiveIndex();
            
            vkCmdPushConstants(cmd, pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
            
            mesh->Bind(cmd);
            vkCmdDrawIndexed(cmd, mesh->GetIndexCount(), 1, 0, 0, 0);
        }
    }

    vkCmdEndRendering(cmd);
}

void VulkanRenderer::RecordLightingCommands(uint32_t frameIndex, VulkanTexture* outputTarget, const std::vector<VulkanTexture*>& gbufferInputs, VulkanTexture* depthInput) {
    VkCommandBuffer cmd = m_graphicsDevice->GetCurrentCommandBuffer();

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = outputTarget->GetImageView();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{0, 0, 0, 1}};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {outputTarget->GetWidth(), outputTarget->GetHeight()}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    // Bind lighting pipeline and G-Buffer textures
    // Lighting pass usually uses a full-screen triangle and samples from G-Buffer via bindless or input attachments.
    // For bindless, we passIndices via Push Constants.
    
    vkCmdEndRendering(cmd);
}

std::shared_ptr<VulkanPipeline> VulkanRenderer::GetOrCreatePipeline(const Material& material, bool isShadowPass) {
    size_t hash = 0; // Simple hash based on material and pass
    if (m_pipelineCache.count(hash)) return m_pipelineCache[hash];

    auto pipeline = std::make_shared<VulkanPipeline>();
    VulkanPipeline::Config config;
    
    // Add shaders from material
    auto vShader = std::static_pointer_cast<VulkanShader>(material.GetVertexShader());
    auto fShader = std::static_pointer_cast<VulkanShader>(material.GetFragmentShader());
    if (vShader) config.shaders.push_back(vShader.get());
    if (fShader) config.shaders.push_back(fShader.get());

    config.descriptorSetLayout = m_globalDescriptorSetLayout;
    
    // Dynamic Rendering Info
    config.renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    if (isShadowPass) {
        config.renderingInfo.depthAttachmentFormat = VulkanUtils::FindDepthFormat(m_graphicsDevice->GetVulkanDevice()->GetPhysicalDevice());
    } else {
        static std::vector<VkFormat> colorFormats = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM };
        config.renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorFormats.size());
        config.renderingInfo.pColorAttachmentFormats = colorFormats.data();
        config.renderingInfo.depthAttachmentFormat = VulkanUtils::FindDepthFormat(m_graphicsDevice->GetVulkanDevice()->GetPhysicalDevice());
    }

    pipeline->Initialize(m_graphicsDevice->GetVulkanDevice(), config);
    m_pipelineCache[hash] = pipeline;
    return pipeline;
}

VulkanTexture* VulkanRenderer::GetGBufferAlbedo() const { return m_renderSubsystem->GetGBufferAlbedo(); }
VulkanTexture* VulkanRenderer::GetGBufferNormal() const { return m_renderSubsystem->GetGBufferNormal(); }
VulkanTexture* VulkanRenderer::GetGBufferPBR() const { return m_renderSubsystem->GetGBufferPBR(); }
VulkanTexture* VulkanRenderer::GetGBufferDepth() const { return m_renderSubsystem->GetGBufferDepth(); }

VkDescriptorSetLayout VulkanRenderer::GetGlobalDescriptorSetLayout() { return m_globalDescriptorSetLayout; }
VkPipelineLayout VulkanRenderer::GetGlobalPipelineLayout() { return m_globalPipelineLayout; }

void VulkanRenderer::ResetInstanceBuffer() {
    m_instanceBufferOffset = 0;
    uint32_t frameIndex = m_graphicsDevice->GetCurrentFrameIndex();
    m_instanceBufferMapped = m_instanceBuffers[frameIndex]->Map();
}

void VulkanRenderer::EndFrame() {
    uint32_t frameIndex = m_graphicsDevice->GetCurrentFrameIndex();
    if (m_instanceBufferMapped) {
        m_instanceBuffers[frameIndex]->Unmap();
        m_instanceBufferMapped = nullptr;
    }
}

} // namespace AstralEngine
