#include "PostProcessingEffectBase.h"
#include "VulkanRenderer.h"
#include <fstream>

namespace AstralEngine {

// Static üyeleri başlat
std::shared_ptr<VulkanBuffer> PostProcessingEffectBase::s_vertexBuffer = nullptr;
uint32_t PostProcessingEffectBase::s_vertexCount = 0;

PostProcessingEffectBase::PostProcessingEffectBase() {
    // Varsayılan konfigürasyonu ayarla
    m_config.name = "PostProcessingEffect";
    m_config.vertexShaderPath = "";
    m_config.fragmentShaderPath = "";
    m_config.frameCount = 3;
    m_config.width = 1920;
    m_config.height = 1080;
    m_config.useMinimalVertexInput = true;
    
    m_frameCount = m_config.frameCount;
    m_width = m_config.width;
    m_height = m_config.height;
}

PostProcessingEffectBase::~PostProcessingEffectBase() {
    Shutdown();
}

bool PostProcessingEffectBase::Initialize(VulkanRenderer* renderer) {
    if (!renderer) {
        SetError("Renderer pointer'ı boş");
        return false;
    }

    m_renderer = renderer;
    m_device = renderer->GetDevice();
    
    if (!m_device) {
        SetError("VulkanDevice alınamadı");
        return false;
    }

    Logger::Info("PostProcessingEffectBase", "Post-processing efekti başlatılıyor...");

    if (!OnInitialize()) {
        return false;
    }
    
    // Create shared resources
    if (!CreateFullScreenQuadBuffer()) {
        Logger::Error("PostProcessingEffectBase", "Failed to create full-screen quad buffer");
        return false;
    }
    
    m_isInitialized = true;
    Logger::Info("PostProcessingEffectBase", "Post-processing efekti başarıyla başlatıldı: " + m_config.name);
    return true;
}

void PostProcessingEffectBase::Shutdown() {
    if (!m_isInitialized) {
        return;
    }

    Logger::Info("PostProcessingEffectBase", "Post-processing efekti kapatılıyor: " + m_config.name);

    OnShutdown();
    
    // Static resources will be automatically managed by shared_ptr
    // No need for manual reference counting

    m_isInitialized = false;
    Logger::Info("PostProcessingEffectBase", "Post-processing efekti başarıyla kapatıldı: " + m_config.name);
}

// RecordCommands metodu kaldırıldı - artık renderer'ın sorumluluğunda

void PostProcessingEffectBase::Update(VulkanTexture* input, uint32_t frameIndex) {
    if (!m_isInitialized || !input) {
        Logger::Error("PostProcessingEffectBase", "Update çağrısı için efekt başlatılmamış veya geçersiz parametreler");
        return;
    }
    
    // Efekt özel update işlemleri burada yapılacak
    // Bu metot türetilmiş sınıflar tarafından override edilecek
}

const std::string& PostProcessingEffectBase::GetName() const {
    return m_config.name;
}

bool PostProcessingEffectBase::IsEnabled() const {
    return m_isEnabled;
}

void PostProcessingEffectBase::SetEnabled(bool enabled) {
    m_isEnabled = enabled;
}

bool PostProcessingEffectBase::LoadShaderSPIRV(const std::string& filepath, std::vector<uint32_t>& spirv) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        SetError("Shader dosyası açılamadı: " + filepath);
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);
    
    spirv.resize(fileSize / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(spirv.data()), fileSize);
    
    return true;
}

bool PostProcessingEffectBase::CreateShaders(const std::string& vertexPath, const std::string& fragmentPath) {
    m_vertexShader = std::make_unique<VulkanShader>();
    m_fragmentShader = std::make_unique<VulkanShader>();
    
    // Shader SPIR-V kodlarını yükle
    std::vector<uint32_t> vertexSPIRV;
    std::vector<uint32_t> fragmentSPIRV;
    
    if (!LoadShaderSPIRV(vertexPath, vertexSPIRV)) {
        return false;
    }
    
    if (!LoadShaderSPIRV(fragmentPath, fragmentSPIRV)) {
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

    return true;
}

bool PostProcessingEffectBase::CreatePipeline() {
    if (!m_vertexShader || !m_fragmentShader) {
        SetError("Shader'lar başlatılmamış");
        return false;
    }

    VulkanPipeline::Config pipelineConfig{};
    pipelineConfig.shaders.push_back(m_vertexShader.get());
    pipelineConfig.shaders.push_back(m_fragmentShader.get());
    pipelineConfig.descriptorSetLayout = m_descriptorSetLayout;
    pipelineConfig.useMinimalVertexInput = m_config.useMinimalVertexInput;
    
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

bool PostProcessingEffectBase::CreateUniformBuffers(size_t uboSize) {
    m_uniformBuffers.resize(m_frameCount);
    
    for (uint32_t i = 0; i < m_frameCount; i++) {
        m_uniformBuffers[i] = std::make_unique<VulkanBuffer>();
        
        VulkanBuffer::Config bufferConfig{};
        bufferConfig.size = uboSize;
        bufferConfig.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        bufferConfig.name = m_config.name + "_UBO_" + std::to_string(i);
        
        if (!m_uniformBuffers[i]->Initialize(m_device, bufferConfig)) {
            SetError("Uniform buffer oluşturulamadı: " + m_uniformBuffers[i]->GetLastError());
            return false;
        }
    }

    return true;
}

bool PostProcessingEffectBase::CreateDescriptorSets(uint32_t samplerDescriptorCount) {
    // Descriptor pool oluştur
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = m_frameCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = m_frameCount * samplerDescriptorCount;

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

bool PostProcessingEffectBase::CreateFullScreenQuadBuffer() {
    if (s_vertexBuffer) {
        s_vertexCount = 6; // Already created
        return true;
    }
    
    // Create the buffer
    auto buffer = std::make_shared<VulkanBuffer>();
    
    // Tam ekran quad vertex verileri
    std::vector<Vertex> vertices = {
        {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f)},  // Sol alt
        {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f)},   // Sağ alt
        {glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f)},   // Sol üst
        {glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f)}     // Sağ üst
    };

    s_vertexCount = static_cast<uint32_t>(vertices.size());

    VulkanBuffer::Config bufferConfig{};
    bufferConfig.size = sizeof(Vertex) * vertices.size();
    bufferConfig.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bufferConfig.name = "PostProcessingQuad";

    if (!buffer->Initialize(m_device, bufferConfig)) {
        Logger::Error("PostProcessingEffectBase", "Vertex buffer oluşturulamadı: " + buffer->GetLastError());
        return false;
    }

    // Vertex verilerini kopyala
    void* data;
    vkMapMemory(m_device->GetDevice(), buffer->GetBufferMemory(), 0, bufferConfig.size, 0, &data);
    memcpy(data, vertices.data(), bufferConfig.size);
    vkUnmapMemory(m_device->GetDevice(), buffer->GetBufferMemory());
    
    s_vertexBuffer = std::move(buffer);
    s_vertexCount = 6;
    return true;
}

// DrawFullScreenQuad metodu kaldırıldı - artık renderer'ın sorumluluğunda

void PostProcessingEffectBase::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("PostProcessingEffectBase", error);
}

const std::string& PostProcessingEffectBase::GetLastError() const {
    return m_lastError;
}

} // namespace AstralEngine