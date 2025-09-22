
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


namespace AstralEngine {

GBufferPass::GBufferPass() {
}

GBufferPass::~GBufferPass() {
}

bool GBufferPass::Initialize(RenderSubsystem* owner) {
    m_owner = owner;
    m_graphicsDevice = owner->GetGraphicsDevice();

    uint32_t width = m_graphicsDevice->GetSwapchain()->GetExtent().width;
    uint32_t height = m_graphicsDevice->GetSwapchain()->GetExtent().height;

    CreateRenderPass();
    CreateFramebuffer(width, height);

    // Create instance buffers for each frame in flight
    m_instanceBuffers.resize(m_graphicsDevice->GetMaxFramesInFlight());
    for (size_t i = 0; i < m_instanceBuffers.size(); ++i) {
        m_instanceBuffers[i] = std::make_unique<VulkanBuffer>(m_graphicsDevice->GetMemoryManager(),
            INSTANCE_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    return true;
}

void GBufferPass::Shutdown() {
    // Wait for the device to be idle before cleaning up
    vkDeviceWaitIdle(m_graphicsDevice->GetDevice());

    CleanupFramebuffer();

    for (auto& pipeline : m_pipelineCache) {
        pipeline.second->Shutdown();
    }
    m_pipelineCache.clear();

    for (auto& layout : m_pipelineLayoutCache) {
        vkDestroyPipelineLayout(m_graphicsDevice->GetDevice(), layout.second, nullptr);
    }
    m_pipelineLayoutCache.clear();

    for (auto& layout : m_descriptorSetLayoutCache) {
        vkDestroyDescriptorSetLayout(m_graphicsDevice->GetDevice(), layout.second, nullptr);
    }
    m_descriptorSetLayoutCache.clear();

    m_instanceBuffers.clear();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_graphicsDevice->GetDevice(), m_renderPass, nullptr);
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

        vkCmdBindVertexBuffers(commandBuffer, 1, 1, &m_instanceBuffers[frameIndex]->GetBuffer(), &m_instanceBufferOffset);
        vkCmdDrawIndexed(commandBuffer, mesh->GetIndexCount(), instances.size(), 0, 0, 0);

        m_instanceBufferOffset += instanceDataSize;
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

    if (vkCreateRenderPass(m_graphicsDevice->GetDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create G-Buffer render pass!");
    }
}

void GBufferPass::CreateFramebuffer(uint32_t width, uint32_t height) {
    m_gBufferAlbedo = std::make_unique<VulkanTexture>(m_graphicsDevice->GetMemoryManager());
    m_gBufferAlbedo->Create(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    m_gBufferNormal = std::make_unique<VulkanTexture>(m_graphicsDevice->GetMemoryManager());
    m_gBufferNormal->Create(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    m_gBufferPBR = std::make_unique<VulkanTexture>(m_graphicsDevice->GetMemoryManager());
    m_gBufferPBR->Create(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    m_gBufferDepth = std::make_unique<VulkanTexture>(m_graphicsDevice->GetMemoryManager());
    m_gBufferDepth->Create(width, height, m_graphicsDevice->GetSwapchain()->GetDepthFormat(), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    std::vector<VkImageView> attachments = {
        m_gBufferAlbedo->GetImageView(),
        m_gBufferNormal->GetImageView(),
        m_gBufferPBR->GetImageView(),
        m_gBufferDepth->GetImageView()
    };

    m_framebuffer = std::make_unique<VulkanFramebuffer>(m_graphicsDevice->GetDevice());
    m_framebuffer->Create(m_renderPass, width, height, attachments);
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
    config.device = m_graphicsDevice->GetDevice();
    config.renderPass = m_renderPass;
    config.vertexShader = material.GetVertexShader();
    config.fragmentShader = material.GetFragmentShader();
    // ... other pipeline configs (vertex input, etc.)

    config.pipelineLayout = GetOrCreatePipelineLayout(GetOrCreateDescriptorSetLayout(material));

    auto pipeline = std::make_shared<VulkanPipeline>();
    pipeline->Initialize(config);
    m_pipelineCache[hash] = pipeline;
    return pipeline;
}

VkDescriptorSetLayout GBufferPass::GetOrCreateDescriptorSetLayout(const Material& material) {
    // This logic would be complex, based on shader reflection
    // For now, returning a dummy or a cached global layout
    return VK_NULL_HANDLE;
}

VkPipelineLayout GBufferPass::GetOrCreatePipelineLayout(VkDescriptorSetLayout descriptorSetLayout) {
    // This logic would also be complex
    return VK_NULL_HANDLE;
}

} // namespace AstralEngine
