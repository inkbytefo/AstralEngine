
#include "GBufferPass.h"
#include "../RenderSubsystem.h"
#include "../GraphicsDevice.h"
#include "../VulkanRenderer.h"
#include "../Core/VulkanFramebuffer.h"
#include "../Buffers/VulkanTexture.h"
#include "../Commands/VulkanPipeline.h"
#include "../Material/Material.h"
#include "../Camera.h"
#include "../../ECS/ECSSubsystem.h"
#include "../VulkanMeshManager.h"
#include "../VulkanTextureManager.h"
#include "../Buffers/VulkanBuffer.h"
#include "../../../Core/Engine.h"
#include "../../Asset/AssetHandle.h"
#include "../../../Core/Logger.h"


namespace AstralEngine {

GBufferPass::GBufferPass() {
}

GBufferPass::~GBufferPass() {
}

bool GBufferPass::Initialize(RenderSubsystem* owner) {
    // Validation
    if (!owner) {
        Logger::Error("GBufferPass::Initialize: Owner is null!");
        return false;
    }
    
    m_owner = owner;
    m_graphicsDevice = owner->GetGraphicsDevice();
    
    if (!m_graphicsDevice) {
        Logger::Error("GBufferPass::Initialize: GraphicsDevice is null!");
        return false;
    }
    
    if (!m_graphicsDevice->IsInitialized()) {
        Logger::Error("GBufferPass::Initialize: GraphicsDevice is not initialized!");
        return false;
    }

    uint32_t width = m_graphicsDevice->GetSwapchain()->GetExtent().width;
    uint32_t height = m_graphicsDevice->GetSwapchain()->GetExtent().height;
    
    if (width == 0 || height == 0) {
        Logger::Error("GBufferPass::Initialize: Invalid swapchain dimensions: %ux%u", width, height);
        return false;
    }

    Logger::Info("Initializing GBufferPass with dimensions: %ux%u", width, height);

    // Get vertex buffer offset alignment from device limits
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_graphicsDevice->GetVulkanDevice()->GetPhysicalDevice(), &deviceProperties);
    m_vertexBufferOffsetAlignment = deviceProperties.limits.minTexelBufferOffsetAlignment;
    if (m_vertexBufferOffsetAlignment == 0) {
        m_vertexBufferOffsetAlignment = 1; // Fallback to 1 if alignment is 0
    }
    Logger::Info("Vertex buffer offset alignment: %zu", m_vertexBufferOffsetAlignment);

    // Create render pass
    CreateRenderPass();
    if (m_renderPass == VK_NULL_HANDLE) {
        Logger::Error("GBufferPass::Initialize: Failed to create render pass!");
        return false;
    }

    // Create framebuffer
    CreateFramebuffer(width, height);
    if (!m_framebuffer || !m_framebuffer->IsInitialized()) {
        Logger::Error("GBufferPass::Initialize: Failed to create framebuffer!");
        return false;
    }

    // Create instance buffers for each frame in flight
    m_instanceBuffers.resize(m_graphicsDevice->GetMaxFramesInFlight());
    for (size_t i = 0; i < m_instanceBuffers.size(); ++i) {
        VulkanBuffer::Config bufferConfig;
        bufferConfig.size = INSTANCE_BUFFER_SIZE;
        bufferConfig.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        m_instanceBuffers[i] = std::make_unique<VulkanBuffer>();
        if (!m_instanceBuffers[i]->Initialize(m_graphicsDevice, bufferConfig)) {
            Logger::Error("Failed to initialize instance buffer %zu: %s", i, m_instanceBuffers[i]->GetLastError().c_str());
            return false;
        }
    }

    Logger::Info("GBufferPass initialized successfully");
    return true;
}

void GBufferPass::Shutdown() {
    // Wait for the device to be idle before cleaning up
    vkDeviceWaitIdle(m_graphicsDevice->GetVulkanDevice()->GetDevice());

    CleanupFramebuffer();

    for (auto& pipeline : m_pipelineCache) {
        pipeline.second->Shutdown();
    }
    m_pipelineCache.clear();

    for (auto& layout : m_pipelineLayoutCache) {
        vkDestroyPipelineLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), layout.second, nullptr);
    }
    m_pipelineLayoutCache.clear();

    for (auto& layout : m_descriptorSetLayoutCache) {
        vkDestroyDescriptorSetLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), layout.second, nullptr);
    }
    m_descriptorSetLayoutCache.clear();

    m_instanceBuffers.clear();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

void GBufferPass::Record(VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    // Reset instance buffer for this frame
    m_instanceBufferOffset = 0;
    m_instanceBufferMapped = m_instanceBuffers[frameIndex]->Map();

    // Get render data from ECS
    ECSSubsystem* ecs = m_owner->GetOwner()->GetSubsystem<ECSSubsystem>();
    auto renderQueue = ecs->GetRenderQueue();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffer->GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_graphicsDevice->GetSwapchain()->GetExtent();

    std::array<VkClearValue, 4> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[3].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_framebuffer->GetWidth();
    viewport.height = (float)m_framebuffer->GetHeight();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_framebuffer->GetWidth(), m_framebuffer->GetHeight()};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VulkanMesh* lastMesh = nullptr;

    for (auto const& [key, instances] : renderQueue) {
        Material* material = m_owner->GetMaterialManager()->GetMaterial(key.materialHandle);
        VulkanMesh* mesh = m_owner->GetVulkanMeshManager()->GetMesh(key.modelHandle);

        if (!material || !mesh || instances.empty()) {
            continue;
        }

        std::shared_ptr<VulkanPipeline> pipeline = GetOrCreatePipeline(*material);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

        // Bind descriptor sets
        // This part needs to be adapted based on how descriptor sets are managed.
        // Assuming a frame-global descriptor set and a material-specific one.
        // VkDescriptorSet sets[] = { m_graphicsDevice->GetCurrentDescriptorSet(frameIndex), material->GetDescriptorSet() };
        // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 2, sets, 0, nullptr);

        if (mesh != lastMesh) {
            VkBuffer vertexBuffers[] = {mesh->GetVertexBuffer()->GetBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            lastMesh = mesh;
        }

        // Copy instance data to buffer
        uint32_t instanceDataSize = sizeof(glm::mat4) * instances.size();
        if (m_instanceBufferOffset + instanceDataSize > INSTANCE_BUFFER_SIZE) {
            // Handle buffer overflow if necessary
            continue;
        }

        memcpy((char*)m_instanceBufferMapped + m_instanceBufferOffset, instances.data(), instanceDataSize);

        // Fix vkCmdBindVertexBuffers call with proper variable types and alignment
        VkBuffer instBuffer = m_instanceBuffers[frameIndex]->GetBuffer();
        VkDeviceSize instOffset = static_cast<VkDeviceSize>(m_instanceBufferOffset);
        vkCmdBindVertexBuffers(commandBuffer, 1, 1, &instBuffer, &instOffset);
        vkCmdDrawIndexed(commandBuffer, mesh->GetIndexCount(), instances.size(), 0, 0, 0);

        m_instanceBufferOffset += instanceDataSize;
        // Align offset for next instance data
        m_instanceBufferOffset = AlignOffset(m_instanceBufferOffset, m_vertexBufferOffsetAlignment);
    }

    vkCmdEndRenderPass(commandBuffer);
    m_instanceBuffers[frameIndex]->Unmap();
}

void GBufferPass::OnResize(uint32_t width, uint32_t height) {
    CleanupFramebuffer();
    CreateFramebuffer(width, height);
}

void GBufferPass::CreateRenderPass() {
    VkAttachmentDescription albedoAttachment{};
    albedoAttachment.format = VK_FORMAT_R8G8B8A8_UNORM; // Albedo (Color)
    albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    albedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    albedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription normalAttachment{};
    normalAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT; // Normals (High Precision)
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription pbrAttachment{};
    pbrAttachment.format = VK_FORMAT_R8G8B8A8_UNORM; // Metallic, Roughness, AO
    pbrAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    pbrAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    pbrAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    pbrAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    pbrAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    pbrAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pbrAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_graphicsDevice->GetSwapchain()->GetDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRefs[] = {
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 3;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 3;
    subpass.pColorAttachments = colorAttachmentRefs;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkAttachmentDescription, 4> attachments = {albedoAttachment, normalAttachment, pbrAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    // Add subpass dependencies
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        Logger::Error("Failed to create G-Buffer render pass!");
        return;
    }
}

void GBufferPass::CreateFramebuffer(uint32_t width, uint32_t height) {
    // Create Albedo texture
    VulkanTexture::Config albedoConfig;
    albedoConfig.width = width;
    albedoConfig.height = height;
    albedoConfig.format = VK_FORMAT_R8G8B8A8_UNORM;
    albedoConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    albedoConfig.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    albedoConfig.name = "GBufferAlbedo";

    m_gBufferAlbedo = std::make_unique<VulkanTexture>();
    if (!m_gBufferAlbedo->Initialize(m_graphicsDevice, albedoConfig)) {
        Logger::Error("Failed to initialize G-Buffer Albedo texture: %s", m_gBufferAlbedo->GetLastError().c_str());
        return;
    }

    // Create Normal texture
    VulkanTexture::Config normalConfig;
    normalConfig.width = width;
    normalConfig.height = height;
    normalConfig.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    normalConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    normalConfig.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    normalConfig.name = "GBufferNormal";

    m_gBufferNormal = std::make_unique<VulkanTexture>();
    if (!m_gBufferNormal->Initialize(m_graphicsDevice, normalConfig)) {
        Logger::Error("Failed to initialize G-Buffer Normal texture: %s", m_gBufferNormal->GetLastError().c_str());
        return;
    }

    // Create PBR texture
    VulkanTexture::Config pbrConfig;
    pbrConfig.width = width;
    pbrConfig.height = height;
    pbrConfig.format = VK_FORMAT_R8G8B8A8_UNORM;
    pbrConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    pbrConfig.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    pbrConfig.name = "GBufferPBR";

    m_gBufferPBR = std::make_unique<VulkanTexture>();
    if (!m_gBufferPBR->Initialize(m_graphicsDevice, pbrConfig)) {
        Logger::Error("Failed to initialize G-Buffer PBR texture: %s", m_gBufferPBR->GetLastError().c_str());
        return;
    }

    // Create Depth texture
    VulkanTexture::Config depthConfig;
    depthConfig.width = width;
    depthConfig.height = height;
    depthConfig.format = m_graphicsDevice->GetSwapchain()->GetDepthFormat();
    depthConfig.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthConfig.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthConfig.name = "GBufferDepth";

    m_gBufferDepth = std::make_unique<VulkanTexture>();
    if (!m_gBufferDepth->Initialize(m_graphicsDevice, depthConfig)) {
        Logger::Error("Failed to initialize G-Buffer Depth texture: %s", m_gBufferDepth->GetLastError().c_str());
        return;
    }

    std::vector<VkImageView> attachments = {
        m_gBufferAlbedo->GetImageView(),
        m_gBufferNormal->GetImageView(),
        m_gBufferPBR->GetImageView(),
        m_gBufferDepth->GetImageView()
    };

    // Create framebuffer with modern API
    VulkanFramebuffer::Config framebufferConfig;
    framebufferConfig.device = m_graphicsDevice->GetVulkanDevice();
    framebufferConfig.renderPass = m_renderPass;
    framebufferConfig.attachments = attachments;
    framebufferConfig.width = width;
    framebufferConfig.height = height;
    framebufferConfig.layers = 1;
    framebufferConfig.name = "GBufferFramebuffer";

    m_framebuffer = std::make_unique<VulkanFramebuffer>();
    if (!m_framebuffer->Initialize(framebufferConfig)) {
        Logger::Error("Failed to initialize G-Buffer framebuffer: %s", m_framebuffer->GetLastError().c_str());
        return;
    }
}

void GBufferPass::CleanupFramebuffer() {
    m_framebuffer.reset();
    m_gBufferAlbedo.reset();
    m_gBufferNormal.reset();
    m_gBufferPBR.reset();
    m_gBufferDepth.reset();
}

// The pipeline and layout cache logic is a simplified version of what was in VulkanRenderer
// In a real scenario, this would need careful implementation.
std::shared_ptr<VulkanPipeline> GBufferPass::GetOrCreatePipeline(const Material& material) {
    // Simplified hash for demonstration
    size_t hash = material.GetShaderHash();

    auto it = m_pipelineCache.find(hash);
    if (it != m_pipelineCache.end()) {
        return it->second;
    }

    VulkanPipelineConfig config{};
    config.device = m_graphicsDevice->GetVulkanDevice()->GetDevice();
    config.renderPass = m_renderPass;
    config.vertexShader = material.GetVertexShader();
    config.fragmentShader = material.GetFragmentShader();
    // ... other pipeline configs (vertex input, etc.)

    VkDescriptorSetLayout descriptorSetLayout = GetOrCreateDescriptorSetLayout(material);
    if (descriptorSetLayout == VK_NULL_HANDLE) {
        Logger::Error("Failed to create descriptor set layout for pipeline!");
        return nullptr;
    }

    config.pipelineLayout = GetOrCreatePipelineLayout(descriptorSetLayout);
    if (config.pipelineLayout == VK_NULL_HANDLE) {
        Logger::Error("Failed to create pipeline layout!");
        return nullptr;
    }

    auto pipeline = std::make_shared<VulkanPipeline>();
    if (!pipeline->Initialize(config)) {
        Logger::Error("Failed to initialize pipeline!");
        return nullptr;
    }
    m_pipelineCache[hash] = pipeline;
    return pipeline;
}

VkDescriptorSetLayout GBufferPass::GetOrCreateDescriptorSetLayout(const Material& material) {
    // Create a hash for the material's descriptor set layout
    size_t hash = material.GetShaderHash();
    
    // Check if we already have this layout cached
    auto it = m_descriptorSetLayoutCache.find(hash);
    if (it != m_descriptorSetLayoutCache.end()) {
        return it->second;
    }
    
    // Create a basic descriptor set layout for G-Buffer pass
    // This typically includes: MVP matrix, material properties, texture samplers
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    
    // Binding 0: Uniform buffer for MVP matrix and camera data
    VkDescriptorSetLayoutBinding mvpBinding{};
    mvpBinding.binding = 0;
    mvpBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    mvpBinding.descriptorCount = 1;
    mvpBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    mvpBinding.pImmutableSamplers = nullptr;
    bindings.push_back(mvpBinding);
    
    // Binding 1: Material properties uniform buffer
    VkDescriptorSetLayoutBinding materialBinding{};
    materialBinding.binding = 1;
    materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialBinding.descriptorCount = 1;
    materialBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    materialBinding.pImmutableSamplers = nullptr;
    bindings.push_back(materialBinding);
    
    // Binding 2-5: Texture samplers (albedo, normal, pbr, emissive)
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 2;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 4; // albedo, normal, pbr, emissive
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerBinding.pImmutableSamplers = nullptr;
    bindings.push_back(samplerBinding);
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        Logger::Error("Failed to create descriptor set layout for material!");
        return VK_NULL_HANDLE;
    }
    
    // Cache the layout
    m_descriptorSetLayoutCache[hash] = descriptorSetLayout;
    Logger::Info("Created descriptor set layout for material (hash: %zu)", hash);
    
    return descriptorSetLayout;
}

VkPipelineLayout GBufferPass::GetOrCreatePipelineLayout(VkDescriptorSetLayout descriptorSetLayout) {
    // Create a hash for the pipeline layout
    size_t hash = std::hash<VkDescriptorSetLayout>{}(descriptorSetLayout);
    
    // Check if we already have this layout cached
    auto it = m_pipelineLayoutCache.find(hash);
    if (it != m_pipelineLayoutCache.end()) {
        return it->second;
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    
    // Push constants for dynamic data (if needed)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 128; // 128 bytes for push constants
    
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(m_graphicsDevice->GetVulkanDevice()->GetDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        Logger::Error("Failed to create pipeline layout!");
        return VK_NULL_HANDLE;
    }
    
    // Cache the layout
    m_pipelineLayoutCache[hash] = pipelineLayout;
    Logger::Info("Created pipeline layout (hash: %zu)", hash);
    
    return pipelineLayout;
}

} // namespace AstralEngine
