#include "TonemappingEffect.h"
#include "VulkanRenderer.h"
#include "Core/VulkanDevice.h"
#include "Buffers/VulkanTexture.h"
#include "../Asset/AssetData.h"
#include <fstream>
#include <filesystem>

namespace AstralEngine {

TonemappingEffect::TonemappingEffect() {
    // Varsayılan tonemapping parametrelerini ayarla
    m_uboData.exposure = 1.0f;
    m_uboData.gamma = 2.2f;
    m_uboData.tonemapper = 1; // ACES
    m_uboData.contrast = 1.0f;
    m_uboData.brightness = 0.0f;
    m_uboData.saturation = 1.0f;
    m_uboData.vignetteIntensity = 0.0f;
    m_uboData.vignetteRadius = 0.5f;
    m_uboData.chromaticAberrationIntensity = 0.0f;
    m_uboData.bloomIntensity = 0.0f;
    m_uboData.lensDirtIntensity = 0.0f;
    m_uboData.useColorGrading = 0;
    m_uboData.useDithering = 0;
    m_uboData.padding = glm::vec2(0.0f, 0.0f);
    
    // Varsayılan push constants
    m_pushConstants.texelSize = glm::vec2(1.0f / 1920.0f, 1.0f / 1080.0f); // Varsayılan çözünürlük
    m_pushConstants.useBloom = 0;
    m_pushConstants.useVignette = 0;
    m_pushConstants.useChromaticAberration = 0;
}

TonemappingEffect::~TonemappingEffect() {
    Shutdown();
}

bool TonemappingEffect::Initialize(VulkanRenderer* renderer) {
    if (!renderer) {
        SetError("Renderer pointer'ı boş");
        return false;
    }

    m_renderer = renderer;
    
    // VulkanDevice'i al (bu kısım renderer'dan alınmalı)
    // Not: Bu kısım VulkanRenderer implementasyonuna bağlı olarak değişebilir
    // Şimdilik geçici olarak nullptr atıyoruz
    m_device = nullptr; // renderer->GetDevice();
    
    if (!m_device) {
        SetError("VulkanDevice alınamadı");
        return false;
    }

    Logger::Info("TonemappingEffect", "Tonemapping efekti başlatılıyor...");

    // Shader'ları yükle
    m_vertexShader = std::make_unique<VulkanShader>();
    m_fragmentShader = std::make_unique<VulkanShader>();
    
    // Shader dosyalarını yükle ve derle
    std::vector<uint32_t> vertexSPIRV;
    std::vector<uint32_t> fragmentSPIRV;
    
    // Shader SPIR-V kodlarını yükle (build sistemi tarafından derlenmiş olmalı)
    if (!LoadShaderSPIRV("Assets/Shaders/PostProcessing/tonemap.vert.spv", vertexSPIRV)) {
        SetError("Vertex shader SPIR-V yüklenemedi");
        return false;
    }
    
    if (!LoadShaderSPIRV("Assets/Shaders/PostProcessing/tonemap.frag.spv", fragmentSPIRV)) {
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

    // Descriptor set layout'ı oluştur
    if (!CreateDescriptorSetLayout()) {
        return false;
    }

    // Pipeline'ı oluştur
    if (!CreatePipeline()) {
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

    // Tam ekran quad vertex buffer'ını oluştur
    CreateFullScreenQuad();

    m_isInitialized = true;
    Logger::Info("TonemappingEffect", "Tonemapping efekti başarıyla başlatıldı");
    return true;
}

void TonemappingEffect::Shutdown() {
    if (!m_isInitialized) {
        return;
    }

    Logger::Info("TonemappingEffect", "Tonemapping efekti kapatılıyor...");

    // Vulkan kaynaklarını ters başlatma sırasıyla temizle
    if (m_vertexBuffer) {
        m_vertexBuffer->Shutdown();
        m_vertexBuffer.reset();
    }

    for (auto& buffer : m_uniformBuffers) {
        if (buffer) {
            buffer->Shutdown();
        }
    }
    m_uniformBuffers.clear();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device->GetDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device->GetDevice(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_pipeline) {
        m_pipeline->Shutdown();
        m_pipeline.reset();
    }

    if (m_fragmentShader) {
        m_fragmentShader->Shutdown();
        m_fragmentShader.reset();
    }

    if (m_vertexShader) {
        m_vertexShader->Shutdown();
        m_vertexShader.reset();
    }

    m_isInitialized = false;
    Logger::Info("TonemappingEffect", "Tonemapping efekti başarıyla kapatıldı");
}

void TonemappingEffect::RecordCommands(VkCommandBuffer commandBuffer, 
                                      VulkanTexture* inputTexture,
                                      VulkanFramebuffer* outputFramebuffer,
                                      uint32_t frameIndex) {
    if (!m_isInitialized || !inputTexture || !outputFramebuffer) {
        Logger::Error("TonemappingEffect", "RecordCommands çağrısı için efekt başlatılmamış veya geçersiz parametreler");
        return;
    }

    // Descriptor set'leri güncelle
    UpdateDescriptorSets(inputTexture);

    // Uniform buffer'ı güncelle
    void* data;
    vkMapMemory(m_device->GetDevice(), m_uniformBuffers[frameIndex]->GetBufferMemory(), 0, sizeof(TonemappingUBO), 0, &data);
    memcpy(data, &m_uboData, sizeof(TonemappingUBO));
    vkUnmapMemory(m_device->GetDevice(), m_uniformBuffers[frameIndex]->GetBufferMemory());

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

    // Descriptor set'leri bağla
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                           m_pipeline->GetLayout(), 0, 1, &m_descriptorSets[frameIndex], 0, nullptr);

    // Push constants'ları ayarla
    vkCmdPushConstants(commandBuffer, m_pipeline->GetLayout(), 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                      0, sizeof(PushConstants), &m_pushConstants);

    // Tam ekran quad'ı çiz
    vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);

    // Render pass'i bitir
    vkCmdEndRenderPass(commandBuffer);
}

const std::string& TonemappingEffect::GetName() const {
    return m_name;
}

bool TonemappingEffect::IsEnabled() const {
    return m_isEnabled;
}

void TonemappingEffect::SetEnabled(bool enabled) {
    m_isEnabled = enabled;
}

bool TonemappingEffect::CreateDescriptorSetLayout() {
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

bool TonemappingEffect::CreatePipeline() {
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

bool TonemappingEffect::CreateUniformBuffers() {
    // Frame sayısını al (swapchain image sayısı)
    uint32_t frameCount = 3; // Varsayılan değer, renderer'dan alınmalı
    
    m_uniformBuffers.resize(frameCount);
    
    for (uint32_t i = 0; i < frameCount; i++) {
        m_uniformBuffers[i] = std::make_unique<VulkanBuffer>();
        
        VulkanBuffer::Config bufferConfig{};
        bufferConfig.size = sizeof(TonemappingUBO);
        bufferConfig.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        bufferConfig.name = "TonemappingUBO_" + std::to_string(i);
        
        if (!m_uniformBuffers[i]->Initialize(m_device, bufferConfig)) {
            SetError("Uniform buffer oluşturulamadı: " + m_uniformBuffers[i]->GetLastError());
            return false;
        }
    }

    return true;
}

bool TonemappingEffect::CreateDescriptorSets() {
    uint32_t frameCount = static_cast<uint32_t>(m_uniformBuffers.size());
    
    // Descriptor pool oluştur
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = frameCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = frameCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = frameCount;

    if (vkCreateDescriptorPool(m_device->GetDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        SetError("Descriptor pool oluşturulamadı");
        return false;
    }

    // Descriptor set'leri allocate et
    std::vector<VkDescriptorSetLayout> layouts(frameCount, m_descriptorSetLayout);
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = frameCount;
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(frameCount);
    if (vkAllocateDescriptorSets(m_device->GetDevice(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        SetError("Descriptor set'ler allocate edilemedi");
        return false;
    }

    return true;
}

void TonemappingEffect::UpdateDescriptorSets(VulkanTexture* inputTexture) {
    for (size_t i = 0; i < m_descriptorSets.size(); i++) {
        // Uniform buffer info
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i]->GetBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(TonemappingUBO);

        // Image info
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = inputTexture->GetImageView();
        imageInfo.sampler = inputTexture->GetSampler();

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->GetDevice(), static_cast<uint32_t>(descriptorWrites.size()), 
                              descriptorWrites.data(), 0, nullptr);
    }
}

void TonemappingEffect::CreateFullScreenQuad() {
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
    bufferConfig.name = "TonemappingQuad";

    if (!m_vertexBuffer->Initialize(m_device, bufferConfig)) {
        Logger::Error("TonemappingEffect", "Vertex buffer oluşturulamadı: " + m_vertexBuffer->GetLastError());
        return;
    }

    // Vertex verilerini kopyala
    void* data;
    vkMapMemory(m_device->GetDevice(), m_vertexBuffer->GetBufferMemory(), 0, bufferConfig.size, 0, &data);
    memcpy(data, vertices.data(), bufferConfig.size);
    vkUnmapMemory(m_device->GetDevice(), m_vertexBuffer->GetBufferMemory());
}

bool TonemappingEffect::LoadShaderSPIRV(const std::string& filepath, std::vector<uint32_t>& spirv) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        Logger::Error("TonemappingEffect", "Shader dosyası açılamadı: " + filepath);
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);
    
    spirv.resize(fileSize / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(spirv.data()), fileSize);
    
    return true;
}

void TonemappingEffect::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("TonemappingEffect", error);
}

} // namespace AstralEngine