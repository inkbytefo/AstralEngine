#include "Material.h"
#include "../../../Core/Logger.h"
#include "../Core/VulkanDevice.h"
#include "../Buffers/VulkanBuffer.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace AstralEngine {

// Material sınıfı implementasyonu
Material::Material() 
    : m_device(nullptr)
    , m_assetManager(nullptr)
    , m_type(MaterialType::PBR)
    , m_name("UnnamedMaterial")
    , m_isInitialized(false) {
    
    Logger::Debug("Material", "Material created");
}

Material::~Material() {
    if (m_isInitialized) {
        Shutdown();
    }
    Logger::Debug("Material", "Material destroyed: {}", m_name);
}

bool Material::Initialize(const Config& config) {
    if (m_isInitialized) {
        Logger::Warning("Material", "Material already initialized: {}", m_name);
        return true;
    }
    
    if (!config.device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    m_device = config.device;
    m_assetManager = config.assetManager;
    m_type = config.type;
    m_name = config.name;
    
    Logger::Info("Material", "Initializing material: {} (type: {})", 
                m_name, static_cast<int>(m_type));
    
    try {
        // Shader'ları yükle
        if (!config.vertexShaderPath.empty() && !config.fragmentShaderPath.empty()) {
            SetShaders(config.vertexShaderPath, config.fragmentShaderPath);
        }
        
        // Descriptor set layout oluştur
        if (!CreateDescriptorSetLayout()) {
            return false;
        }
        
        // Pipeline layout oluştur
        if (!CreatePipelineLayout()) {
            return false;
        }
        
        // Descriptor pool oluştur
        if (!CreateDescriptorPool()) {
            return false;
        }
        
        // Descriptor sets oluştur
        if (!CreateDescriptorSets()) {
            return false;
        }
        
        // Uniform buffer'lar oluştur
        m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDeviceSize bufferSize = sizeof(MaterialProperties);
            
            if (!VulkanBuffer::CreateBuffer(
                m_device,
                bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                m_uniformBuffers[i],
                m_uniformBuffersMemory[i]
            )) {
                SetError("Failed to create uniform buffer");
                return false;
            }
            
            // Buffer'ı map et
            vkMapMemory(m_device->GetDevice(), m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
        }
        
        m_isInitialized = true;
        Logger::Info("Material", "Material initialized successfully: {}", m_name);
        return true;
        
    } catch (const std::exception& e) {
        SetError(std::string("Exception during material initialization: ") + e.what());
        Logger::Error("Material", "Material initialization failed: {}", e.what());
        return false;
    }
}

void Material::Shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    Logger::Info("Material", "Shutting down material: {}", m_name);
    
    // Uniform buffer'ları temizle
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (m_uniformBuffersMapped[i]) {
            vkUnmapMemory(m_device->GetDevice(), m_uniformBuffersMemory[i]);
        }
        if (m_uniformBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->GetDevice(), m_uniformBuffers[i], nullptr);
        }
        if (m_uniformBuffersMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->GetDevice(), m_uniformBuffersMemory[i], nullptr);
        }
    }
    
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();
    
    // Vulkan kaynaklarını temizle
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device->GetDevice(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device->GetDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device->GetDevice(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    
    m_descriptorSets.clear();
    m_textureSlots.clear();
    m_textureMap.clear();
    
    m_vertexShader.reset();
    m_fragmentShader.reset();
    
    m_device = nullptr;
    m_assetManager = nullptr;
    m_isInitialized = false;
    
    Logger::Info("Material", "Material shutdown completed: {}", m_name);
}

void Material::SetProperties(const MaterialProperties& props) {
    m_properties = props;
    
    // Transparent flag'ini güncelle
    m_properties.transparent = (m_properties.opacity < 1.0f);
    
    Logger::Debug("Material", "Material properties updated for: {}", m_name);
}

void Material::SetTexture(TextureType type, const std::string& texturePath) {
    if (!m_assetManager) {
        Logger::Warning("Material", "AssetManager not available for texture loading: {}", texturePath);
        return;
    }
    
    auto texture = m_assetManager->LoadVulkanTexture(texturePath, m_device);
    if (texture) {
        SetTexture(type, texture);
        Logger::Info("Material", "Texture loaded: {} -> {}", GetTextureName(type), texturePath);
    } else {
        Logger::Error("Material", "Failed to load texture: {}", texturePath);
    }
}

void Material::SetTexture(TextureType type, std::shared_ptr<VulkanTexture> texture) {
    if (!texture) {
        Logger::Warning("Material", "Invalid texture pointer for type: {}", static_cast<int>(type));
        return;
    }
    
    // Texture slot'ını bul veya oluştur
    auto it = m_textureMap.find(type);
    if (it != m_textureMap.end()) {
        // Mevcut texture'ı güncelle
        size_t index = it->second;
        m_textureSlots[index].texture = texture;
        m_textureSlots[index].enabled = true;
    } else {
        // Yeni texture slot'u ekle
        TextureSlot slot;
        slot.texture = texture;
        slot.type = type;
        slot.name = GetTextureName(type);
        slot.binding = GetTextureBinding(type);
        slot.enabled = true;
        
        m_textureSlots.push_back(slot);
        m_textureMap[type] = m_textureSlots.size() - 1;
    }
    
    // Descriptor set'i güncelle
    UpdateDescriptorSet();
    
    Logger::Debug("Material", "Texture set: {} -> {}", GetTextureName(type), texture ? "valid" : "null");
}

std::shared_ptr<VulkanTexture> Material::GetTexture(TextureType type) const {
    auto it = m_textureMap.find(type);
    if (it != m_textureMap.end()) {
        size_t index = it->second;
        return m_textureSlots[index].texture;
    }
    return nullptr;
}

void Material::RemoveTexture(TextureType type) {
    auto it = m_textureMap.find(type);
    if (it != m_textureMap.end()) {
        size_t index = it->second;
        m_textureSlots[index].texture.reset();
        m_textureSlots[index].enabled = false;
        
        // Descriptor set'i güncelle
        UpdateDescriptorSet();
        
        Logger::Debug("Material", "Texture removed: {}", GetTextureName(type));
    }
}

bool Material::HasTexture(TextureType type) const {
    auto it = m_textureMap.find(type);
    if (it != m_textureMap.end()) {
        size_t index = it->second;
        return m_textureSlots[index].enabled && m_textureSlots[index].texture != nullptr;
    }
    return false;
}

const TextureSlot* Material::GetTextureSlot(TextureType type) const {
    auto it = m_textureMap.find(type);
    if (it != m_textureMap.end()) {
        size_t index = it->second;
        return &m_textureSlots[index];
    }
    return nullptr;
}

void Material::SetShaders(const std::string& vertexPath, const std::string& fragmentPath) {
    if (!m_assetManager) {
        Logger::Warning("Material", "AssetManager not available for shader loading");
        return;
    }
    
    // Vertex shader'ı yükle
    m_vertexShader = m_assetManager->LoadShader(vertexPath, m_device);
    if (!m_vertexShader) {
        Logger::Error("Material", "Failed to load vertex shader: {}", vertexPath);
    }
    
    // Fragment shader'ı yükle
    m_fragmentShader = m_assetManager->LoadShader(fragmentPath, m_device);
    if (!m_fragmentShader) {
        Logger::Error("Material", "Failed to load fragment shader: {}", fragmentPath);
    }
    
    Logger::Info("Material", "Shaders loaded for material: {}", m_name);
}

void Material::UpdateDescriptorSet() {
    if (!m_isInitialized || m_descriptorSet == VK_NULL_HANDLE) {
        return;
    }
    
    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorImageInfo> imageInfos;
    
    // Texture descriptor'ları
    for (const auto& slot : m_textureSlots) {
        if (slot.enabled && slot.texture) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = slot.texture->GetImageView();
            imageInfo.sampler = slot.texture->GetSampler();
            
            imageInfos.push_back(imageInfo);
            
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSet;
            descriptorWrite.dstBinding = slot.binding;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfos.back();
            
            descriptorWrites.push_back(descriptorWrite);
        }
    }
    
    // Uniform buffer descriptor'ı
    if (m_currentFrame < m_uniformBuffers.size()) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[m_currentFrame];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(MaterialProperties);
        
        VkWriteDescriptorSet bufferWrite{};
        bufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bufferWrite.dstSet = m_descriptorSet;
        bufferWrite.dstBinding = 0; // Material properties binding
        bufferWrite.dstArrayElement = 0;
        bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufferWrite.descriptorCount = 1;
        bufferWrite.pBufferInfo = &bufferInfo;
        
        descriptorWrites.push_back(bufferWrite);
    }
    
    if (!descriptorWrites.empty()) {
        vkUpdateDescriptorSets(m_device->GetDevice(), 
                              static_cast<uint32_t>(descriptorWrites.size()), 
                              descriptorWrites.data(), 0, nullptr);
        
        Logger::Debug("Material", "Descriptor set updated for material: {}", m_name);
    }
}

void Material::UpdateUniformBuffer(uint32_t currentFrame) {
    if (!m_isInitialized || currentFrame >= m_uniformBuffersMapped.size()) {
        return;
    }
    
    m_currentFrame = currentFrame;
    
    // Uniform buffer'ı güncelle
    memcpy(m_uniformBuffersMapped[currentFrame], &m_properties, sizeof(MaterialProperties));
    
    Logger::Debug("Material", "Uniform buffer updated for material: {}", m_name);
}

bool Material::CreateDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    
    // Material properties uniform buffer (binding = 0)
    VkDescriptorSetLayoutBinding materialBinding{};
    materialBinding.binding = 0;
    materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialBinding.descriptorCount = 1;
    materialBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    materialBinding.pImmutableSamplers = nullptr;
    bindings.push_back(materialBinding);
    
    // Texture bindings
    for (const auto& slot : m_textureSlots) {
        if (slot.enabled) {
            VkDescriptorSetLayoutBinding textureBinding{};
            textureBinding.binding = slot.binding;
            textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textureBinding.descriptorCount = 1;
            textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            textureBinding.pImmutableSamplers = nullptr;
            bindings.push_back(textureBinding);
        }
    }
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    VkResult result = vkCreateDescriptorSetLayout(m_device->GetDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout);
    if (result != VK_SUCCESS) {
        SetError("Failed to create descriptor set layout: " + std::to_string(result));
        return false;
    }
    
    Logger::Debug("Material", "Descriptor set layout created for material: {}", m_name);
    return true;
}

bool Material::CreateDescriptorPool() {
    std::vector<VkDescriptorPoolSize> poolSizes;
    
    // Uniform buffer pool size
    VkDescriptorPoolSize uniformPoolSize{};
    uniformPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformPoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;
    poolSizes.push_back(uniformPoolSize);
    
    // Texture pool size
    VkDescriptorPoolSize texturePoolSize{};
    texturePoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texturePoolSize.descriptorCount = static_cast<uint32_t>(m_textureSlots.size()) * MAX_FRAMES_IN_FLIGHT;
    poolSizes.push_back(texturePoolSize);
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    
    VkResult result = vkCreateDescriptorPool(m_device->GetDevice(), &poolInfo, nullptr, &m_descriptorPool);
    if (result != VK_SUCCESS) {
        SetError("Failed to create descriptor pool: " + std::to_string(result));
        return false;
    }
    
    Logger::Debug("Material", "Descriptor pool created for material: {}", m_name);
    return true;
}

bool Material::CreateDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();
    
    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VkResult result = vkAllocateDescriptorSets(m_device->GetDevice(), &allocInfo, m_descriptorSets.data());
    if (result != VK_SUCCESS) {
        SetError("Failed to allocate descriptor sets: " + std::to_string(result));
        return false;
    }
    
    // İlk descriptor set'i kullan
    m_descriptorSet = m_descriptorSets[0];
    
    Logger::Debug("Material", "Descriptor sets created for material: {}", m_name);
    return true;
}

bool Material::CreatePipelineLayout() {
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;
    
    VkResult result = vkCreatePipelineLayout(m_device->GetDevice(), &layoutInfo, nullptr, &m_pipelineLayout);
    if (result != VK_SUCCESS) {
        SetError("Failed to create pipeline layout: " + std::to_string(result));
        return false;
    }
    
    Logger::Debug("Material", "Pipeline layout created for material: {}", m_name);
    return true;
}

void Material::UpdateTextureBindings() {
    m_textureMap.clear();
    
    for (size_t i = 0; i < m_textureSlots.size(); i++) {
        if (m_textureSlots[i].enabled) {
            m_textureMap[m_textureSlots[i].type] = i;
        }
    }
}

uint32_t Material::GetTextureBinding(TextureType type) const {
    // Texture binding numaralarını döndür
    switch (type) {
        case TextureType::Albedo: return 1;
        case TextureType::Normal: return 2;
        case TextureType::Metallic: return 3;
        case TextureType::Roughness: return 4;
        case TextureType::AO: return 5;
        case TextureType::Emissive: return 6;
        case TextureType::Opacity: return 7;
        case TextureType::Displacement: return 8;
        case TextureType::Custom: return 9;
        default: return 1;
    }
}

std::string Material::GetTextureName(TextureType type) const {
    switch (type) {
        case TextureType::Albedo: return "Albedo";
        case TextureType::Normal: return "Normal";
        case TextureType::Metallic: return "Metallic";
        case TextureType::Roughness: return "Roughness";
        case TextureType::AO: return "AO";
        case TextureType::Emissive: return "Emissive";
        case TextureType::Opacity: return "Opacity";
        case TextureType::Displacement: return "Displacement";
        case TextureType::Custom: return "Custom";
        default: return "Unknown";
    }
}

void Material::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("Material", "Error in material {}: {}", m_name, error);
}

// MaterialManager sınıfı implementasyonu
MaterialManager::MaterialManager() 
    : m_device(nullptr)
    , m_assetManager(nullptr)
    , m_initialized(false) {
    
    Logger::Debug("MaterialManager", "MaterialManager created");
}

MaterialManager::~MaterialManager() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("MaterialManager", "MaterialManager destroyed");
}

bool MaterialManager::Initialize(VulkanDevice* device, AssetManager* assetManager) {
    if (m_initialized) {
        Logger::Warning("MaterialManager", "MaterialManager already initialized");
        return true;
    }
    
    if (!device || !assetManager) {
        Logger::Error("MaterialManager", "Invalid device or asset manager pointer");
        return false;
    }
    
    m_device = device;
    m_assetManager = assetManager;
    
    Logger::Info("MaterialManager", "Initializing MaterialManager");
    
    try {
        // Varsayılan materyalleri oluştur
        if (!CreateDefaultMaterials()) {
            Logger::Error("MaterialManager", "Failed to create default materials");
            return false;
        }
        
        m_initialized = true;
        Logger::Info("MaterialManager", "MaterialManager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("MaterialManager", "MaterialManager initialization failed: {}", e.what());
        return false;
    }
}

void MaterialManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("MaterialManager", "Shutting down MaterialManager");
    
    // Tüm materyalleri temizle
    m_materials.clear();
    m_defaultPBRMaterial.reset();
    m_defaultUnlitMaterial.reset();
    
    m_device = nullptr;
    m_assetManager = nullptr;
    m_initialized = false;
    
    Logger::Info("MaterialManager", "MaterialManager shutdown completed");
}

void MaterialManager::Update() {
    if (!m_initialized) {
        return;
    }
    
    // Kullanılmayan materyalleri temizle
    CleanupUnusedMaterials();
}

std::shared_ptr<Material> MaterialManager::CreateMaterial(const Material::Config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        auto material = std::make_shared<Material>();
        if (!material->Initialize(config)) {
            Logger::Error("MaterialManager", "Failed to create material: {}", config.name);
            return nullptr;
        }
        
        // Materyali kaydet
        m_materials[config.name] = material;
        
        Logger::Info("MaterialManager", "Material created: {}", config.name);
        return material;
        
    } catch (const std::exception& e) {
        Logger::Error("MaterialManager", "Exception creating material: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<Material> MaterialManager::LoadMaterial(const std::string& materialPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Önce cache'te ara
    auto it = m_materials.find(materialPath);
    if (it != m_materials.end()) {
        return it->second;
    }
    
    try {
        // JSON dosyasını oku
        std::ifstream file(materialPath);
        if (!file.is_open()) {
            Logger::Error("MaterialManager", "Failed to open material file: {}", materialPath);
            return nullptr;
        }
        
        json materialJson;
        file >> materialJson;
        
        // Materyal config'ini oluştur
        Material::Config config;
        config.name = materialJson.value("name", "UnnamedMaterial");
        config.type = static_cast<MaterialType>(materialJson.value("type", 0));
        config.vertexShaderPath = materialJson.value("vertexShader", "");
        config.fragmentShaderPath = materialJson.value("fragmentShader", "");
        config.device = m_device;
        config.assetManager = m_assetManager;
        
        // Materyali oluştur
        auto material = std::make_shared<Material>();
        if (!material->Initialize(config)) {
            Logger::Error("MaterialManager", "Failed to initialize material from file: {}", materialPath);
            return nullptr;
        }
        
        // Properties'leri yükle
        if (materialJson.contains("properties")) {
            const auto& props = materialJson["properties"];
            MaterialProperties materialProps;
            
            if (props.contains("baseColor")) {
                const auto& color = props["baseColor"];
                materialProps.baseColor = glm::vec3(color[0], color[1], color[2]);
            }
            materialProps.metallic = props.value("metallic", 0.0f);
            materialProps.roughness = props.value("roughness", 0.5f);
            materialProps.ao = props.value("ao", 1.0f);
            materialProps.opacity = props.value("opacity", 1.0f);
            materialProps.transparent = props.value("transparent", false);
            
            material->SetProperties(materialProps);
        }
        
        // Texture'ları yükle
        if (materialJson.contains("textures")) {
            const auto& textures = materialJson["textures"];
            for (const auto& texture : textures.items()) {
                std::string typeStr = texture.key();
                std::string path = texture.value();
                
                TextureType type = TextureType::Albedo; // Default
                if (typeStr == "albedo") type = TextureType::Albedo;
                else if (typeStr == "normal") type = TextureType::Normal;
                else if (typeStr == "metallic") type = TextureType::Metallic;
                else if (typeStr == "roughness") type = TextureType::Roughness;
                else if (typeStr == "ao") type = TextureType::AO;
                else if (typeStr == "emissive") type = TextureType::Emissive;
                else if (typeStr == "opacity") type = TextureType::Opacity;
                
                material->SetTexture(type, path);
            }
        }
        
        // Materyali kaydet
        m_materials[config.name] = material;
        
        Logger::Info("MaterialManager", "Material loaded from file: {}", materialPath);
        return material;
        
    } catch (const std::exception& e) {
        Logger::Error("MaterialManager", "Exception loading material: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<Material> MaterialManager::GetMaterial(const std::string& materialName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_materials.find(materialName);
    if (it != m_materials.end()) {
        return it->second;
    }
    
    Logger::Warning("MaterialManager", "Material not found: {}", materialName);
    return nullptr;
}

void MaterialManager::RegisterMaterial(const std::string& name, std::shared_ptr<Material> material) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (material) {
        m_materials[name] = material;
        Logger::Info("MaterialManager", "Material registered: {}", name);
    }
}

void MaterialManager::UnregisterMaterial(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_materials.find(name);
    if (it != m_materials.end()) {
        m_materials.erase(it);
        Logger::Info("MaterialManager", "Material unregistered: {}", name);
    }
}

bool MaterialManager::HasMaterial(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_materials.find(name) != m_materials.end();
}

void MaterialManager::ClearUnusedMaterials() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Reference count 1 olan materyalleri temizle (sadece manager'da referans var)
    auto it = m_materials.begin();
    while (it != m_materials.end()) {
        if (it->second.use_count() == 1) {
            Logger::Debug("MaterialManager", "Removing unused material: {}", it->first);
            it = m_materials.erase(it);
        } else {
            ++it;
        }
    }
}

bool MaterialManager::CreateDefaultMaterials() {
    // Default PBR Material
    Material::Config pbrConfig;
    pbrConfig.name = "DefaultPBR";
    pbrConfig.type = MaterialType::PBR;
    pbrConfig.vertexShaderPath = "Assets/Shaders/Materials/pbr_material.vert";
    pbrConfig.fragmentShaderPath = "Assets/Shaders/Materials/pbr_material.frag";
    pbrConfig.device = m_device;
    pbrConfig.assetManager = m_assetManager;
    
    m_defaultPBRMaterial = std::make_shared<Material>();
    if (!m_defaultPBRMaterial->Initialize(pbrConfig)) {
        Logger::Error("MaterialManager", "Failed to create default PBR material");
        return false;
    }
    
    // Default Unlit Material
    Material::Config unlitConfig;
    unlitConfig.name = "DefaultUnlit";
    unlitConfig.type = MaterialType::Unlit;
    unlitConfig.vertexShaderPath = "Assets/Shaders/Materials/unlit.vert";
    unlitConfig.fragmentShaderPath = "Assets/Shaders/Materials/unlit.frag";
    unlitConfig.device = m_device;
    unlitConfig.assetManager = m_assetManager;
    
    m_defaultUnlitMaterial = std::make_shared<Material>();
    if (!m_defaultUnlitMaterial->Initialize(unlitConfig)) {
        Logger::Error("MaterialManager", "Failed to create default unlit material");
        return false;
    }
    
    // Materyalleri kaydet
    m_materials["DefaultPBR"] = m_defaultPBRMaterial;
    m_materials["DefaultUnlit"] = m_defaultUnlitMaterial;
    
    Logger::Info("MaterialManager", "Default materials created successfully");
    return true;
}

void MaterialManager::CleanupUnusedMaterials() {
    // Bu metod periyodik olarak çağrılır
    ClearUnusedMaterials();
}

} // namespace AstralEngine
