#include "BloomEffect.h"
#include "VulkanRenderer.h"
#include "Core/VulkanDevice.h"
#include "Buffers/VulkanTexture.h"
#include "../Asset/AssetData.h"
#include <fstream>
#include <filesystem>
#include <array> // For std::array

namespace AstralEngine {

BloomEffect::BloomEffect() {
    // Varsayılan bloom parametrelerini ayarla
    m_uboData.threshold = 1.0f;
    m_uboData.knee = 0.5f;
    m_uboData.intensity = 0.5f;
    m_uboData.radius = 4.0f;
    m_uboData.quality = 1; // Medium quality
    m_uboData.useDirt = 0;
    m_uboData.dirtIntensity = 0.0f;
    m_uboData.padding = glm::vec2(0.0f, 0.0f);
    
    // Varsayılan push constants
    m_pushConstants.texelSize = glm::vec2(1.0f / 1920.0f, 1.0f / 1080.0f); // Varsayılan çözünürlük
    m_pushConstants.bloomPass = 0; // Bright pass
    
    // Temel sınıf konfigürasyonunu ayarla
    SetName("BloomEffect");
}

BloomEffect::~BloomEffect() {
    Shutdown();
}

bool BloomEffect::Initialize(VulkanRenderer* renderer) {
    // Temel sınıfın Initialize metodunu çağır
    return PostProcessingEffectBase::Initialize(renderer);
}

void BloomEffect::Shutdown() {
    // Temel sınıfın Shutdown metodunu çağır
    PostProcessingEffectBase::Shutdown();
}

void BloomEffect::Update(VulkanTexture* inputTexture, uint32_t frameIndex) {
    if (!m_isInitialized || !inputTexture) {
        Logger::Error("BloomEffect", "Update çağrısı için efekt başlatılmamış veya geçersiz parametreler");
        return;
    }

    // Uniform buffer'ı güncelle
    void* data;
    vkMapMemory(GetDevice()->GetDevice(), GetUniformBuffers()[frameIndex]->GetMemory(), 0, sizeof(BloomUBO), 0, &data);
    memcpy(data, &m_uboData, sizeof(BloomUBO));
    vkUnmapMemory(GetDevice()->GetDevice(), GetUniformBuffers()[frameIndex]->GetMemory());

    // Descriptor set'leri güncelle
    UpdateDescriptorSets(inputTexture, frameIndex);
}

const std::string& BloomEffect::GetName() const {
    return m_config.name;
}

bool BloomEffect::IsEnabled() const {
    return m_isEnabled;
}

void BloomEffect::SetEnabled(bool enabled) {
    m_isEnabled = enabled;
}

bool BloomEffect::OnInitialize() {
    Logger::Info("BloomEffect", "Bloom efektinin özel başlatma işlemleri başlatılıyor...");

    // Shader'ları yükle - BloomEffect'e özel shader yollarını kullan
    if (!CreateShaders("Assets/Shaders/PostProcessing/bloom.vert.spv", "Assets/Shaders/PostProcessing/bloom.frag.spv")) {
        return false;
    }

    // Bloom efektine özel descriptor set layout'ı oluştur
    if (!CreateDescriptorSetLayout()) {
        return false;
    }

    // Pipeline'ı oluştur
    if (!CreatePipeline()) {
        return false;
    }

    // Uniform buffer'ları oluştur (BloomUBO boyutunda)
    if (!CreateUniformBuffers(sizeof(BloomUBO))) {
        return false;
    }

    // Descriptor set'leri oluştur (Bloom için 4 sampler descriptor: input, bright pass, blur x2)
    if (!CreateDescriptorSets(4)) {
        return false;
    }

    // Ara texture'ları oluştur
    if (!CreateIntermediateTextures()) {
        return false;
    }

    // Ara framebuffer'ları oluştur
    if (!CreateIntermediateFramebuffers()) {
        return false;
    }

    Logger::Info("BloomEffect", "Bloom efektinin özel başlatma işlemleri tamamlandı");
    return true;
}

void BloomEffect::OnShutdown() {
    Logger::Info("BloomEffect", "Bloom efektinin özel kapatma işlemleri başlatılıyor...");

    // Vulkan kaynaklarını ters başlatma sırasıyla temizle
    
    // Ara framebuffer'ları temizle
    for (auto& framebuffer : m_blurFramebuffers) {
        if (framebuffer) {
            framebuffer->Shutdown();
        }
    }
    m_blurFramebuffers.clear();

    for (auto& framebuffer : m_brightPassFramebuffers) {
        if (framebuffer) {
            framebuffer->Shutdown();
        }
    }
    m_brightPassFramebuffers.clear();

    // Render pass'leri temizle
    if (m_blurRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(GetDevice()->GetDevice(), m_blurRenderPass, nullptr);
        m_blurRenderPass = VK_NULL_HANDLE;
    }

    if (m_brightPassRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(GetDevice()->GetDevice(), m_brightPassRenderPass, nullptr);
        m_brightPassRenderPass = VK_NULL_HANDLE;
    }

    // Ara texture'ları temizle
    for (auto& texture : m_blurTextures) {
        if (texture) {
            texture->Shutdown();
        }
    }
    m_blurTextures.clear();

    for (auto& texture : m_brightPassTextures) {
        if (texture) {
            texture->Shutdown();
        }
    }
    m_brightPassTextures.clear();

    Logger::Info("BloomEffect", "Bloom efektinin özel kapatma işlemleri tamamlandı");
}


bool BloomEffect::CreateDescriptorSetLayout() {
    // Uniform buffer binding
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    // Sampler binding
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    // Descriptor set layout'ı temel sınıf aracılığıyla oluştur
    // Not: Bu metodun PostProcessingEffectBase'de implemente edilmesi gerekiyor
    // Şimdilik doğrudan oluşturuyoruz
    if (vkCreateDescriptorSetLayout(GetDevice()->GetDevice(), &layoutInfo, nullptr, const_cast<VkDescriptorSetLayout*>(&GetDescriptorSetLayout())) != VK_SUCCESS) {
        SetError("Descriptor set layout oluşturulamadı");
        return false;
    }

    return true;
}

void BloomEffect::UpdateDescriptorSets(VulkanTexture* inputTexture, uint32_t frameIndex) {
    if (!inputTexture || frameIndex >= GetDescriptorSets().size()) {
        return;
    }

    // Uniform buffer info
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = GetUniformBuffers()[frameIndex]->GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(BloomUBO);

    // Input texture info
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = inputTexture->GetImageView();
    imageInfo.sampler = inputTexture->GetSampler();

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    // Uniform buffer descriptor'ı güncelle
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = GetDescriptorSets()[frameIndex];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    // Sampler descriptor'ı güncelle
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = GetDescriptorSets()[frameIndex];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(GetDevice()->GetDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                          descriptorWrites.data(), 0, nullptr);
}

void BloomEffect::UpdateDescriptorSetsForPass(VulkanTexture* inputTexture, uint32_t frameIndex) {
    // Belirli bir bloom pass'i için descriptor set'leri güncelle
    if (!inputTexture || frameIndex >= GetDescriptorSets().size()) {
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = inputTexture->GetImageView();
    imageInfo.sampler = GetTextureSampler();

    std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

    // Sampler descriptor'ı güncelle
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = GetDescriptorSets()[frameIndex];
    descriptorWrites[0].dstBinding = 1;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(GetDevice()->GetDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void BloomEffect::UpdateCompositeDescriptorSets(VulkanTexture* originalTexture, VulkanTexture* bloomTexture, uint32_t frameIndex) {
    // Composite pass için iki texture'ı da güncelle (orijinal sahne ve bloom texture)
    if (!originalTexture || !bloomTexture || frameIndex >= GetDescriptorSets().size()) {
        return;
    }

    // Not: Bu metodun implementasyonu, composite pass için iki texture'ı aynı anda bağlamalıdır.
    // Gerçek implementasyonda descriptor set layout'ı iki sampler binding'i desteklemelidir.
    // Şimdilik sadece bloom texture'ı güncelliyoruz.
    UpdateDescriptorSetsForPass(bloomTexture, frameIndex);
}

void BloomEffect::RecordPass(VkCommandBuffer commandBuffer, VulkanFramebuffer* targetFramebuffer,
                             VulkanTexture* inputTexture, BloomPass passType, uint32_t frameIndex) {
    m_pushConstants.bloomPass = static_cast<int>(passType);
    
    // Update descriptor sets to bind the correct input texture for this pass
    if (passType != BloomPass::Composite) {
        UpdateDescriptorSetsForPass(inputTexture, frameIndex);
    }
    
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = targetFramebuffer->GetRenderPass(); // Assumes framebuffers share compatible render passes
    renderPassInfo.framebuffer = targetFramebuffer->GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {targetFramebuffer->GetWidth(), targetFramebuffer->GetHeight()};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline()->GetPipeline());
    
    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {GetVertexBuffer()->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    
    // Bind descriptor sets...
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           GetPipeline()->GetLayout(), 0, 1, &GetDescriptorSets()[frameIndex], 0, nullptr);
    
    vkCmdPushConstants(commandBuffer, GetPipeline()->GetLayout(),
                      VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, GetVertexCount(), 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
}

bool BloomEffect::CreateIntermediateTextures() {
    // Bright pass texture'ları oluştur - tam çözünürlükte
    m_brightPassTextures.resize(m_frameCount);
    for (uint32_t i = 0; i < m_frameCount; i++) {
        m_brightPassTextures[i] = std::make_unique<VulkanTexture>();
        
        VulkanTexture::Config textureConfig{};
        textureConfig.width = m_width;
        textureConfig.height = m_height;
        textureConfig.format = VK_FORMAT_R16G16B16A16_SFLOAT; // Yüksek dinamik aralık için
        textureConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        textureConfig.name = "BloomBrightPass_" + std::to_string(i);
        
        if (!m_brightPassTextures[i]->Initialize(GetDevice(), textureConfig)) {
            SetError("Bright pass texture oluşturulamadı: " + m_brightPassTextures[i]->GetLastError());
            return false;
        }
    }

    // Blur texture'ları oluştur - downsampling ile mip chain desteği
    m_blurTextures.resize(m_frameCount);
    for (uint32_t i = 0; i < m_frameCount; i++) {
        m_blurTextures[i] = std::make_unique<VulkanTexture>();
        
        VulkanTexture::Config textureConfig{};
        textureConfig.width = m_width / 2; // Start at half resolution for downsampling optimization
        textureConfig.height = m_height / 2;
        textureConfig.mipLevels = 5; // Create 5 mip levels for progressive blurring
        textureConfig.format = VK_FORMAT_R16G16B16A16_SFLOAT; // Yüksek dinamik aralık için
        textureConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        textureConfig.name = "BloomBlurMipChain_" + std::to_string(i);
        
        if (!m_blurTextures[i]->Initialize(GetDevice(), textureConfig)) {
            SetError("Blur mip-chain texture oluşturulamadı: " + m_blurTextures[i]->GetLastError());
            return false;
        }
    }

    // Not: Gerçek bir implementasyonda her mip level için ayrı image view'lar ve framebuffer'lar oluşturulmalıdır.
    // Bu, daha karmaşık bir mimari değişikliği gerektirir ve şu anki örnekte basitleştirilmiştir.
    // Tam bir downsampling implementasyonu için:
    // 1. Her mip level için VkImageView oluşturulmalı
    // 2. Her mip level için ayrı framebuffer'lar oluşturulmalı
    // 3. Blur pass'leri farklı mip seviyeleri arasında geçiş yapmalıdır

    return true;
}

bool BloomEffect::CreateIntermediateFramebuffers() {
    // Render pass'leri oluştur
    // Bu kısım basitleştirilmiştir, gerçek implementasyonda daha detaylı olmalı
    
    // Bright pass render pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

    if (vkCreateRenderPass(GetDevice()->GetDevice(), &renderPassInfo, nullptr, &m_brightPassRenderPass) != VK_SUCCESS) {
        SetError("Bright pass render pass oluşturulamadı");
        return false;
    }

    // Blur render pass (aynı yapı)
    if (vkCreateRenderPass(GetDevice()->GetDevice(), &renderPassInfo, nullptr, &m_blurRenderPass) != VK_SUCCESS) {
        SetError("Blur render pass oluşturulamadı");
        return false;
    }

    // Framebuffer'ları oluştur
    m_brightPassFramebuffers.resize(m_frameCount);
    for (uint32_t i = 0; i < m_frameCount; i++) {
        VulkanFramebuffer::Config framebufferConfig{};
        framebufferConfig.device = GetDevice();
        framebufferConfig.renderPass = m_brightPassRenderPass;
        framebufferConfig.attachments = {m_brightPassTextures[i]->GetImageView()};
        framebufferConfig.width = m_width;
        framebufferConfig.height = m_height;
        framebufferConfig.name = "BrightPassFramebuffer_" + std::to_string(i);
        
        m_brightPassFramebuffers[i] = std::make_unique<VulkanFramebuffer>();
        if (!m_brightPassFramebuffers[i]->Initialize(framebufferConfig)) {
            SetError("Bright pass framebuffer oluşturulamadı");
            return false;
        }
    }

    m_blurFramebuffers.resize(m_frameCount);
    for (uint32_t i = 0; i < m_frameCount; i++) {
        VulkanFramebuffer::Config framebufferConfig{};
        framebufferConfig.device = GetDevice();
        framebufferConfig.renderPass = m_blurRenderPass;
        framebufferConfig.attachments = {m_blurTextures[i]->GetImageView()};
        framebufferConfig.width = m_width;
        framebufferConfig.height = m_height;
        framebufferConfig.name = "BlurFramebuffer_" + std::to_string(i);
        
        m_blurFramebuffers[i] = std::make_unique<VulkanFramebuffer>();
        if (!m_blurFramebuffers[i]->Initialize(framebufferConfig)) {
            SetError("Blur framebuffer oluşturulamadı");
            return false;
        }
    }

    return true;
}

void BloomEffect::RecordBrightPass(VkCommandBuffer commandBuffer, VulkanTexture* inputTexture, uint32_t frameIndex) {
    m_pushConstants.bloomPass = static_cast<int>(BloomPass::BrightPass);
    
    // Render pass başlat
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_brightPassRenderPass;
    renderPassInfo.framebuffer = m_brightPassFramebuffers[frameIndex]->GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_width, m_height};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Pipeline'ı bağla
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline()->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {GetVertexBuffer()->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla (input texture ile)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //                        GetPipeline()->GetLayout(), 0, 1, &GetDescriptorSets()[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, GetPipeline()->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, GetVertexCount(), 1, 0, 0);

    // Render pass'i bitir
    vkCmdEndRenderPass(commandBuffer);
}

void BloomEffect::RecordHorizontalBlur(VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    m_pushConstants.bloomPass = static_cast<int>(BloomPass::HorizontalBlur);
    
    // Render pass başlat
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_blurRenderPass;
    renderPassInfo.framebuffer = m_blurFramebuffers[frameIndex]->GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_width, m_height};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Pipeline'ı bağla
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline()->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {GetVertexBuffer()->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla (bright pass texture ile)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //                        GetPipeline()->GetLayout(), 0, 1, &GetDescriptorSets()[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, GetPipeline()->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, GetVertexCount(), 1, 0, 0);

    // Render pass'i bitir
    vkCmdEndRenderPass(commandBuffer);
}

void BloomEffect::RecordVerticalBlur(VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    m_pushConstants.bloomPass = static_cast<int>(BloomPass::VerticalBlur);
    
    // Render pass başlat
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_blurRenderPass;
    renderPassInfo.framebuffer = m_brightPassFramebuffers[frameIndex]->GetFramebuffer(); // Bright pass framebuffer'ını yeniden kullan
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {m_width, m_height};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Pipeline'ı bağla
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline()->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {GetVertexBuffer()->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla (horizontal blur texture ile)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //                        GetPipeline()->GetLayout(), 0, 1, &GetDescriptorSets()[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, GetPipeline()->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, GetVertexCount(), 1, 0, 0);

    // Render pass'i bitir
    vkCmdEndRenderPass(commandBuffer);
}

void BloomEffect::RecordComposite(VkCommandBuffer commandBuffer, VulkanTexture* inputTexture, 
                                 VulkanFramebuffer* outputFramebuffer, uint32_t frameIndex) {
    m_pushConstants.bloomPass = static_cast<int>(BloomPass::Composite);
    
    // Render pass başlat
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = outputFramebuffer->GetRenderPass(); // Bu metodun olması gerekiyor
    renderPassInfo.framebuffer = outputFramebuffer->GetFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {outputFramebuffer->GetWidth(), outputFramebuffer->GetHeight()};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Pipeline'ı bağla
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline()->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {GetVertexBuffer()->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla (input texture ve blur texture ile)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //                        GetPipeline()->GetLayout(), 0, 1, &GetDescriptorSets()[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, GetPipeline()->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, GetVertexCount(), 1, 0, 0);

    // Render pass'i bitir
    vkCmdEndRenderPass(commandBuffer);
}

void BloomEffect::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("BloomEffect", error);
}

} // namespace AstralEngine
