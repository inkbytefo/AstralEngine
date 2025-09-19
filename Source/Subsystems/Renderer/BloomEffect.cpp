#include "BloomEffect.h"
#include "VulkanRenderer.h"
#include "Core/VulkanDevice.h"
#include "Buffers/VulkanTexture.h"
#include "../Asset/AssetData.h"
#include <fstream>
#include <filesystem>

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
}

BloomEffect::~BloomEffect() {
    Shutdown();
}

bool BloomEffect::Initialize(VulkanRenderer* renderer) {
    if (!renderer) {
        SetError("Renderer pointer'ı boş");
        return false;
    }

    m_renderer = renderer;
    
    // VulkanDevice'i al (bu kısım renderer'dan alınmalı)
    // Not: Bu kısım VulkanRenderer implementasyonuna bağlı olarak değişebilir
    m_device = nullptr; // renderer->GetDevice();
    
    if (!m_device) {
        SetError("VulkanDevice alınamadı");
        return false;
    }

    Logger::Info("BloomEffect", "Bloom efekti başlatılıyor...");

    // Shader'ları yükle
    m_vertexShader = std::make_unique<VulkanShader>();
    m_fragmentShader = std::make_unique<VulkanShader>();
    
    // Shader SPIR-V kodlarını yükle (build sistemi tarafından derlenmiş olmalı)
    std::vector<uint32_t> vertexSPIRV;
    std::vector<uint32_t> fragmentSPIRV;
    
    if (!LoadShaderSPIRV("Assets/Shaders/PostProcessing/bloom.vert.spv", vertexSPIRV)) {
        SetError("Vertex shader SPIR-V yüklenemedi");
        return false;
    }
    
    if (!LoadShaderSPIRV("Assets/Shaders/PostProcessing/bloom.frag.spv", fragmentSPIRV)) {
        SetError("Fragment shader SPIR-V yüklenemedi");
        return false;
    }
    
    // Shader modüllerini oluştur
    if (!m_vertexShader->Initialize(m_device, vertexSPIRV, VK_SHADER_STAGE_VERTEX_BIT)) {
        SetError("Vertex shader başlatılamadı: " + m_vertexShader->GetLastError());
        return false;
    }
    
    if (!m_fragmentShader->Initialize(m_device, fragmentSPIRV, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        SetError("Fragment shader başlatılamadı: " + m_fragmentShader->GetLastError());
        return false;
    }

    // Swapchain boyutlarını al
    // m_width = renderer->GetSwapchainExtent().width;
    // m_height = renderer->GetSwapchainExtent().height;
    // m_frameCount = renderer->GetSwapchainImageCount();

    // Descriptor set layout'ları oluştur
    if (!CreateDescriptorSetLayouts()) {
        return false;
    }

    // Pipeline'ı oluştur
    if (!CreatePipelines()) {
        return false;
    }

    // Uniform buffer'ları oluştur
    if (!CreateUniformBuffers()) {
        return false;
    }

    // Descriptor set'leri oluştur
    if (!CreateDescriptorSets()) {
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

    // Tam ekran quad vertex buffer'ını oluştur
    CreateFullScreenQuad();

    m_isInitialized = true;
    Logger::Info("BloomEffect", "Bloom efekti başarıyla başlatıldı");
    return true;
}

void BloomEffect::Shutdown() {
    if (!m_isInitialized) {
        return;
    }

    Logger::Info("BloomEffect", "Bloom efekti kapatılıyor...");

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
        vkDestroyRenderPass(m_device->GetDevice(), m_blurRenderPass, nullptr);
        m_blurRenderPass = VK_NULL_HANDLE;
    }

    if (m_brightPassRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device->GetDevice(), m_brightPassRenderPass, nullptr);
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

    // Vertex buffer'ı temizle
    if (m_vertexBuffer) {
        m_vertexBuffer->Shutdown();
        m_vertexBuffer.reset();
    }

    // Uniform buffer'ları temizle
    for (auto& buffer : m_uniformBuffers) {
        if (buffer) {
            buffer->Shutdown();
        }
    }
    m_uniformBuffers.clear();

    // Descriptor pool'u temizle
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device->GetDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    // Descriptor set layout'ı temizle
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device->GetDevice(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    // Pipeline'ı temizle
    if (m_pipeline) {
        m_pipeline->Shutdown();
        m_pipeline.reset();
    }

    // Shader'ları temizle
    if (m_fragmentShader) {
        m_fragmentShader->Shutdown();
        m_fragmentShader.reset();
    }

    if (m_vertexShader) {
        m_vertexShader->Shutdown();
        m_vertexShader.reset();
    }

    m_isInitialized = false;
    Logger::Info("BloomEffect", "Bloom efekti başarıyla kapatıldı");
}

void BloomEffect::RecordCommands(VkCommandBuffer commandBuffer, 
                                VulkanTexture* inputTexture,
                                VulkanFramebuffer* outputFramebuffer,
                                uint32_t frameIndex) {
    if (!m_isInitialized || !inputTexture || !outputFramebuffer) {
        Logger::Error("BloomEffect", "RecordCommands çağrısı için efekt başlatılmamış veya geçersiz parametreler");
        return;
    }

    // Descriptor set'leri güncelle
    UpdateDescriptorSets(inputTexture, frameIndex);

    // Uniform buffer'ı güncelle
    void* data;
    vkMapMemory(m_device->GetDevice(), m_uniformBuffers[frameIndex]->GetBufferMemory(), 0, sizeof(BloomUBO), 0, &data);
    memcpy(data, &m_uboData, sizeof(BloomUBO));
    vkUnmapMemory(m_device->GetDevice(), m_uniformBuffers[frameIndex]->GetBufferMemory());

    // Bloom pass'lerini sırayla uygula
    RecordBrightPass(commandBuffer, inputTexture, frameIndex);
    RecordHorizontalBlur(commandBuffer, frameIndex);
    RecordVerticalBlur(commandBuffer, frameIndex);
    RecordComposite(commandBuffer, inputTexture, outputFramebuffer, frameIndex);
}

const std::string& BloomEffect::GetName() const {
    return m_name;
}

bool BloomEffect::IsEnabled() const {
    return m_isEnabled;
}

void BloomEffect::SetEnabled(bool enabled) {
    m_isEnabled = enabled;
}

bool BloomEffect::CreateDescriptorSetLayouts() {
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

    if (vkCreateDescriptorSetLayout(m_device->GetDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        SetError("Descriptor set layout oluşturulamadı");
        return false;
    }

    return true;
}

bool BloomEffect::CreatePipelines() {
    VulkanPipeline::Config pipelineConfig{};
    pipelineConfig.shaders.push_back(m_vertexShader.get());
    pipelineConfig.shaders.push_back(m_fragmentShader.get());
    pipelineConfig.descriptorSetLayout = m_descriptorSetLayout;
    pipelineConfig.useMinimalVertexInput = true; // Tam ekran quad için minimal vertex input
    
    // Swapchain ve extent bilgileri renderer'dan alınmalı
    // pipelineConfig.swapchain = m_renderer->GetSwapchain();
    // pipelineConfig.extent = m_renderer->GetSwapchainExtent();

    m_pipeline = std::make_unique<VulkanPipeline>();
    if (!m_pipeline->Initialize(m_device, pipelineConfig)) {
        SetError("Pipeline oluşturulamadı: " + m_pipeline->GetLastError());
        return false;
    }

    return true;
}

bool BloomEffect::CreateUniformBuffers() {
    m_uniformBuffers.resize(m_frameCount);
    
    for (uint32_t i = 0; i < m_frameCount; i++) {
        m_uniformBuffers[i] = std::make_unique<VulkanBuffer>();
        
        VulkanBuffer::Config bufferConfig{};
        bufferConfig.size = sizeof(BloomUBO);
        bufferConfig.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        bufferConfig.name = "BloomUBO_" + std::to_string(i);
        
        if (!m_uniformBuffers[i]->Initialize(m_device, bufferConfig)) {
            SetError("Uniform buffer oluşturulamadı: " + m_uniformBuffers[i]->GetLastError());
            return false;
        }
    }

    return true;
}

bool BloomEffect::CreateDescriptorSets() {
    // Descriptor pool oluştur
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = m_frameCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = m_frameCount * 4; // Her frame için 4 texture (input, bright pass, blur x2)

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = m_frameCount;

    if (vkCreateDescriptorPool(m_device->GetDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        SetError("Descriptor pool oluşturulamadı");
        return false;
    }

    // Descriptor set'leri allocate et
    std::vector<VkDescriptorSetLayout> layouts(m_frameCount, m_descriptorSetLayout);
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = m_frameCount;
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(m_frameCount);
    if (vkAllocateDescriptorSets(m_device->GetDevice(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        SetError("Descriptor set'ler allocate edilemedi");
        return false;
    }

    return true;
}

bool BloomEffect::CreateIntermediateTextures() {
    // Bright pass texture'ları oluştur
    m_brightPassTextures.resize(m_frameCount);
    for (uint32_t i = 0; i < m_frameCount; i++) {
        m_brightPassTextures[i] = std::make_unique<VulkanTexture>();
        
        VulkanTexture::Config textureConfig{};
        textureConfig.width = m_width;
        textureConfig.height = m_height;
        textureConfig.format = VK_FORMAT_R16G16B16A16_SFLOAT; // Yüksek dinamik aralık için
        textureConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        textureConfig.name = "BloomBrightPass_" + std::to_string(i);
        
        if (!m_brightPassTextures[i]->Initialize(m_device, textureConfig)) {
            SetError("Bright pass texture oluşturulamadı: " + m_brightPassTextures[i]->GetLastError());
            return false;
        }
    }

    // Blur texture'ları oluştur
    m_blurTextures.resize(m_frameCount);
    for (uint32_t i = 0; i < m_frameCount; i++) {
        m_blurTextures[i] = std::make_unique<VulkanTexture>();
        
        VulkanTexture::Config textureConfig{};
        textureConfig.width = m_width;
        textureConfig.height = m_height;
        textureConfig.format = VK_FORMAT_R16G16B16A16_SFLOAT; // Yüksek dinamik aralık için
        textureConfig.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        textureConfig.name = "BloomBlur_" + std::to_string(i);
        
        if (!m_blurTextures[i]->Initialize(m_device, textureConfig)) {
            SetError("Blur texture oluşturulamadı: " + m_blurTextures[i]->GetLastError());
            return false;
        }
    }

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

    if (vkCreateRenderPass(m_device->GetDevice(), &renderPassInfo, nullptr, &m_brightPassRenderPass) != VK_SUCCESS) {
        SetError("Bright pass render pass oluşturulamadı");
        return false;
    }

    // Blur render pass (aynı yapı)
    if (vkCreateRenderPass(m_device->GetDevice(), &renderPassInfo, nullptr, &m_blurRenderPass) != VK_SUCCESS) {
        SetError("Blur render pass oluşturulamadı");
        return false;
    }

    // Framebuffer'ları oluştur
    m_brightPassFramebuffers.resize(m_frameCount);
    for (uint32_t i = 0; i < m_frameCount; i++) {
        VulkanFramebuffer::Config framebufferConfig{};
        framebufferConfig.device = m_device;
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
        framebufferConfig.device = m_device;
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

void BloomEffect::UpdateDescriptorSets(VulkanTexture* inputTexture, uint32_t frameIndex) {
    // Bu metod her bloom pass'i için ayrı ayrı çağrılmalı
    // Şimdilik basitleştirilmiş implementasyon
}

void BloomEffect::CreateFullScreenQuad() {
    // Tam ekran quad vertex verileri - AssetData.h'daki Vertex yapısını kullan
    std::vector<Vertex> vertices = {
        {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f)},  // Sol alt
        {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f)},   // Sağ alt
        {glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f)},   // Sol üst
        {glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f)}     // Sağ üst
    };

    m_vertexCount = static_cast<uint32_t>(vertices.size());

    // Vertex buffer'ı oluştur
    m_vertexBuffer = std::make_unique<VulkanBuffer>();
    
    VulkanBuffer::Config bufferConfig{};
    bufferConfig.size = sizeof(Vertex) * vertices.size();
    bufferConfig.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bufferConfig.name = "BloomQuad";

    if (!m_vertexBuffer->Initialize(m_device, bufferConfig)) {
        Logger::Error("BloomEffect", "Vertex buffer oluşturulamadı: " + m_vertexBuffer->GetLastError());
        return;
    }

    // Vertex verilerini kopyala
    void* data;
    vkMapMemory(m_device->GetDevice(), m_vertexBuffer->GetBufferMemory(), 0, bufferConfig.size, 0, &data);
    memcpy(data, vertices.data(), bufferConfig.size);
    vkUnmapMemory(m_device->GetDevice(), m_vertexBuffer->GetBufferMemory());
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
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {m_vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla (input texture ile)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //                        m_pipeline->GetLayout(), 0, 1, &m_descriptorSets[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, m_pipeline->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);

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
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {m_vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla (bright pass texture ile)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //                        m_pipeline->GetLayout(), 0, 1, &m_descriptorSets[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, m_pipeline->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);

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
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {m_vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla (horizontal blur texture ile)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //                        m_pipeline->GetLayout(), 0, 1, &m_descriptorSets[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, m_pipeline->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);

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
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());

    // Vertex buffer'ı bağla
    VkBuffer vertexBuffers[] = {m_vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Descriptor set'leri bağla (input texture ve blur texture ile)
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    //                        m_pipeline->GetLayout(), 0, 1, &m_descriptorSets[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, m_pipeline->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);

    // Render pass'i bitir
    vkCmdEndRenderPass(commandBuffer);
}

bool BloomEffect::LoadShaderSPIRV(const std::string& filepath, std::vector<uint32_t>& spirv) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        Logger::Error("BloomEffect", "Shader dosyası açılamadı: " + filepath);
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);
    
    spirv.resize(fileSize / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(spirv.data()), fileSize);
    
    return true;
}

void BloomEffect::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("BloomEffect", error);
}

} // namespace AstralEngine