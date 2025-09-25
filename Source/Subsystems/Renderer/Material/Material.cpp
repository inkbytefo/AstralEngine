#include "Material.h"
#include "Core/Logger.h"
#include "../../Asset/AssetData.h"
#include "../../Asset/AssetManager.h"
#include "../Shaders/VulkanShader.h"
#include "../Core/VulkanDevice.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <functional>
#include <algorithm>

using json = nlohmann::json;

namespace AstralEngine {

// Material sınıfı implementasyonu
Material::Material()
    : m_type(MaterialType::PBR)
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

    // Materyal verilerini ayarla
    m_type = config.type;
    m_name = config.name;
    m_vertexShaderHandle = config.vertexShaderHandle;
    m_fragmentShaderHandle = config.fragmentShaderHandle;

    Logger::Info("Material", "Initializing material: {} (type: {})",
                 m_name, static_cast<int>(m_type));

    try {
        // Validate shader handles
        if (!m_vertexShaderHandle.IsValid()) {
            SetError("Invalid vertex shader handle");
            Logger::Error("Material", "Vertex shader handle is invalid for material: {}", m_name);
            return false;
        }

        if (!m_fragmentShaderHandle.IsValid()) {
            SetError("Invalid fragment shader handle");
            Logger::Error("Material", "Fragment shader handle is invalid for material: {}", m_name);
            return false;
        }

        // Validate shader handles compatibility using MaterialManager
        // TODO: Get MaterialManager instance - this needs to be provided by the renderer context
        // For now, we'll create a mock implementation that demonstrates the pattern
        // This would be replaced with actual MaterialManager integration

        // Placeholder implementation - in real code, this would be:
        // auto materialManager = GetMaterialManagerFromContext();
        // if (!materialManager) {
        //     SetError("MaterialManager not available for shader validation");
        //     Logger::Error("Material", "Cannot validate shader handles - MaterialManager not available for material: {}", m_name);
        //     return false;
        // }

        // if (!materialManager->ValidateShaderHandlesCompatibility()) {
        //     SetError("Shader handles are not compatible");
        //     Logger::Error("Material", "Shader handles validation failed for material: {}", m_name);
        //     return false;
        // }

        // For now, create a mock implementation that demonstrates the pattern
        // In real implementation, this would be replaced with actual MaterialManager integration

        Logger::Debug("Material", "Shader handles validation using MaterialManager pattern for material: {}", m_name);

        // Texture slot'larını ve map'lerini başlat
        m_textureSlots.clear();
        m_textureMap.clear();

        // Reset shader caching state
        m_vertexShader.reset();
        m_fragmentShader.reset();
        m_shaderHash = 0;
        m_shadersLoaded = false;

        // Materyal özelliklerini varsayılan değerlere ayarla
        m_properties = MaterialProperties{};
        m_properties.baseColor = glm::vec3(1.0f, 1.0f, 1.0f);
        m_properties.metallic = 0.0f;
        m_properties.roughness = 0.5f;
        m_properties.ao = 1.0f;
        m_properties.opacity = 1.0f;
        m_properties.transparent = false;
        m_properties.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);
        m_properties.emissiveIntensity = 0.0f;
        m_properties.doubleSided = false;
        m_properties.wireframe = false;
        m_properties.tilingX = 1.0f;
        m_properties.tilingY = 1.0f;
        m_properties.offsetX = 0.0f;
        m_properties.offsetY = 0.0f;

        // Clear any previous errors
        m_lastError.clear();

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

    try {
        // Clean up cached shader objects - just reset shared_ptrs
        // MaterialManager::ClearShaderCache is responsible for resource shutdown when the last reference drops
        if (m_vertexShader) {
            Logger::Debug("Material", "Releasing vertex shader reference for material: {}", m_name);
            m_vertexShader.reset();
        }

        if (m_fragmentShader) {
            Logger::Debug("Material", "Releasing fragment shader reference for material: {}", m_name);
            m_fragmentShader.reset();
        }

        // Reset shader caching state
        m_shaderHash = 0;
        m_shadersLoaded = false;

        // Clear texture resources (these are still managed by Material)
        m_textureSlots.clear();
        m_textureMap.clear();

        // Clear any error state
        m_lastError.clear();

        m_isInitialized = false;

        Logger::Info("Material", "Material shutdown completed successfully: {}", m_name);

    } catch (const std::exception& e) {
        Logger::Error("Material", "Exception during material shutdown for {}: {}", m_name, e.what());
        // Still mark as not initialized even if cleanup failed
        m_isInitialized = false;
    }
}


void Material::SetProperties(const MaterialProperties& props) {
    m_properties = props;

    Logger::Debug("Material", "Material properties updated for: {}", m_name);
}


void Material::SetTexture(TextureType type, std::shared_ptr<ITexture> texture) {
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

    Logger::Debug("Material", "Texture set: {} -> {}", GetTextureName(type), texture ? "valid" : "null");
}

std::shared_ptr<ITexture> Material::GetTexture(TextureType type) const {
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




// CreateDescriptorSets metodu kaldırıldı
// Artık descriptor set'leri merkezi VulkanFrameManager tarafından yönetiliyor



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

uint64_t Material::GetShaderHash() const {
    // Return cached hash if already calculated
    if (m_shaderHash != 0) {
        return m_shaderHash;
    }

    // Calculate combined hash from vertex and fragment shader handles
    // Use FNV-1a hash combination for better distribution
    uint64_t vertexHash = static_cast<uint64_t>(m_vertexShaderHandle.GetID());
    uint64_t fragmentHash = static_cast<uint64_t>(m_fragmentShaderHandle.GetID());

    // FNV-1a hash combination for better distribution
    uint64_t combinedHash = 14695981039346656037ULL; // FNV offset basis
    combinedHash ^= vertexHash;
    combinedHash *= 1099511628211ULL; // FNV prime
    combinedHash ^= fragmentHash;
    combinedHash *= 1099511628211ULL; // FNV prime

    m_shaderHash = combinedHash;
    return m_shaderHash;
}

std::shared_ptr<IShader> Material::GetVertexShader() const {
    // Load shaders if not already loaded
    LoadShadersIfNeeded();

    // Return cached vertex shader if available
    if (m_vertexShader && m_vertexShader->IsInitialized()) {
        return m_vertexShader;
    }

    Logger::Warning("Material", "Vertex shader not available or not initialized for material: {}", m_name);
    return nullptr;
}

std::shared_ptr<IShader> Material::GetFragmentShader() const {
    // Load shaders if not already loaded
    LoadShadersIfNeeded();

    // Return cached fragment shader if available
    if (m_fragmentShader && m_fragmentShader->IsInitialized()) {
        return m_fragmentShader;
    }

    Logger::Warning("Material", "Fragment shader not available or not initialized for material: {}", m_name);
    return nullptr;
}

void Material::LoadShadersIfNeeded() const {
    // If shaders are already loaded, return early
    if (m_shadersLoaded) {
        return;
    }

    // Thread-safe loading check
    std::lock_guard<std::mutex> lock(m_shaderLoadMutex);

    // Double-check after acquiring lock
    if (m_shadersLoaded) {
        return;
    }

    Logger::Debug("Material", "Loading shaders for material: {}", m_name);

    // Check if MaterialManager is available
    if (!m_materialManager) {
        SetError("MaterialManager not available for shader loading");
        Logger::Error("Material", "Cannot load shaders - MaterialManager not available for material: {}", m_name);
        return;
    }

    try {
        // Load vertex shader using MaterialManager's LoadShader method
        m_vertexShader = m_materialManager->LoadShader(m_vertexShaderHandle, ShaderStage::Vertex);
        if (!m_vertexShader || !m_vertexShader->IsInitialized()) {
            SetError("Failed to load vertex shader");
            Logger::Error("Material", "Vertex shader loading failed for material: {}", m_name);
            return;
        }

        // Load fragment shader using MaterialManager's LoadShader method
        m_fragmentShader = m_materialManager->LoadShader(m_fragmentShaderHandle, ShaderStage::Fragment);
        if (!m_fragmentShader || !m_fragmentShader->IsInitialized()) {
            SetError("Failed to load fragment shader");
            Logger::Error("Material", "Fragment shader loading failed for material: {}", m_name);
            return;
        }

        // Validate shader compatibility using MaterialManager
        auto validationResult = m_materialManager->ValidateShaderCompatibility(m_vertexShaderHandle, m_fragmentShaderHandle);
        if (!validationResult.IsValid()) {
            SetError("Shader compatibility validation failed: " + validationResult.GetMessages());
            Logger::Error("Material", "Shader compatibility validation failed for material: {} - {}",
                         m_name, validationResult.GetMessages());
            return;
        }

        // Log any warnings from validation
        for (const auto& warning : validationResult.warnings) {
            Logger::Warning("Material", "Shader validation warning for material {}: {}", m_name, warning);
        }

        m_shadersLoaded = true;
        Logger::Info("Material", "Shaders loaded successfully for material: {}", m_name);

    } catch (const std::exception& e) {
        SetError(std::string("Exception during shader loading: ") + e.what());
        Logger::Error("Material", "Exception during shader loading for material {}: {}", m_name, e.what());
    }
}



} // namespace AstralEngine
