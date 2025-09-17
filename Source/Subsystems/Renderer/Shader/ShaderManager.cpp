#include "ShaderManager.h"
#include "../../Core/Logger.h"
#include "../Shaders/ShaderCompiler.h"
#include "../Shaders/VulkanShader.h"
#include <filesystem>

namespace AstralEngine {

ShaderManager::ShaderManager() {
    Logger::Debug("ShaderManager", "ShaderManager created");
}

ShaderManager::~ShaderManager() {
    Logger::Debug("ShaderManager", "ShaderManager destroyed");
}

bool ShaderManager::Initialize(VulkanDevice* device, AssetManager* assetManager, TextureManager* textureManager) {
    if (!device || !assetManager || !textureManager) {
        Logger::Error("ShaderManager", "Invalid parameters provided");
        return false;
    }
    
    m_device = device;
    m_assetManager = assetManager;
    m_textureManager = textureManager;
    
    // Shader compiler'ı başlat
    m_shaderCompiler = std::make_unique<ShaderCompiler>();
    if (!m_shaderCompiler->Initialize(device)) {
        Logger::Error("ShaderManager", "Failed to initialize shader compiler");
        return false;
    }
    
    Logger::Info("ShaderManager", "Shader manager initialized successfully");
    return true;
}

void ShaderManager::Shutdown() {
    // Tüm shader'ları temizle
    m_shaders.clear();
    
    if (m_shaderCompiler) {
        m_shaderCompiler->Shutdown();
        m_shaderCompiler.reset();
    }
    
    m_device = nullptr;
    m_assetManager = nullptr;
    m_textureManager = nullptr;
    
    Logger::Info("ShaderManager", "Shader manager shutdown complete");
}

void ShaderManager::Update() {
    // Shader hot-reload kontrolü
    CheckForShaderUpdates();
    
    // Cache temizleme
    CleanupUnusedShaders();
}

std::shared_ptr<VulkanShader> ShaderManager::LoadShader(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string shaderKey = vertexPath + "|" + fragmentPath;
    
    // Cache'te var mı kontrol et
    auto it = m_shaders.find(shaderKey);
    if (it != m_shaders.end()) {
        Logger::Debug("ShaderManager", "Shader found in cache: {}", shaderKey);
        return it->second;
    }
    
    Logger::Info("ShaderManager", "Loading shader: {}", shaderKey);
    
    // Vertex shader'ı derle
    auto vertexSpirv = m_shaderCompiler->CompileShader(vertexPath, ShaderType::Vertex);
    if (vertexSpirv.empty()) {
        Logger::Error("ShaderManager", "Failed to compile vertex shader: {}", vertexPath);
        return nullptr;
    }
    
    // Fragment shader'ı derle
    auto fragmentSpirv = m_shaderCompiler->CompileShader(fragmentPath, ShaderType::Fragment);
    if (fragmentSpirv.empty()) {
        Logger::Error("ShaderManager", "Failed to compile fragment shader: {}", fragmentPath);
        return nullptr;
    }
    
    // VulkanShader oluştur
    auto shader = std::make_shared<VulkanShader>();
    if (!shader->Initialize(m_device, vertexSpirv, fragmentSpirv)) {
        Logger::Error("ShaderManager", "Failed to create Vulkan shader: {}", shaderKey);
        return nullptr;
    }
    
    // Shader'a meta veri ekle
    ShaderMetadata metadata;
    metadata.vertexPath = vertexPath;
    metadata.fragmentPath = fragmentPath;
    metadata.lastModified = std::filesystem::last_write_time(vertexPath);
    
    // Cache'e ekle
    m_shaders[shaderKey] = shader;
    m_shaderMetadata[shader.get()] = metadata;
    
    Logger::Info("ShaderManager", "Shader loaded successfully: {}", shaderKey);
    return shader;
}

std::shared_ptr<VulkanShader> ShaderManager::LoadComputeShader(const std::string& computePath) {
    std::string shaderKey = "compute|" + computePath;
    
    // Cache'te var mı kontrol et
    auto it = m_shaders.find(shaderKey);
    if (it != m_shaders.end()) {
        Logger::Debug("ShaderManager", "Compute shader found in cache: {}", shaderKey);
        return it->second;
    }
    
    Logger::Info("ShaderManager", "Loading compute shader: {}", computePath);
    
    // Compute shader'ı derle
    auto computeSpirv = m_shaderCompiler->CompileShader(computePath, ShaderType::Compute);
    if (computeSpirv.empty()) {
        Logger::Error("ShaderManager", "Failed to compile compute shader: {}", computePath);
        return nullptr;
    }
    
    // VulkanShader oluştur
    auto shader = std::make_shared<VulkanShader>();
    if (!shader->InitializeCompute(m_device, computeSpirv)) {
        Logger::Error("ShaderManager", "Failed to create Vulkan compute shader: {}", shaderKey);
        return nullptr;
    }
    
    // Shader'a meta veri ekle
    ShaderMetadata metadata;
    metadata.computePath = computePath;
    metadata.lastModified = std::filesystem::last_write_time(computePath);
    
    // Cache'e ekle
    m_shaders[shaderKey] = shader;
    m_shaderMetadata[shader.get()] = metadata;
    
    Logger::Info("ShaderManager", "Compute shader loaded successfully: {}", shaderKey);
    return shader;
}

void ShaderManager::ReloadShader(VulkanShader* shader) {
    if (!shader) {
        Logger::Error("ShaderManager", "Invalid shader provided for reload");
        return;
    }
    
    // Meta veriyi bul
    auto it = m_shaderMetadata.find(shader);
    if (it == m_shaderMetadata.end()) {
        Logger::Error("ShaderManager", "Shader metadata not found for reload");
        return;
    }
    
    const auto& metadata = it->second;
    Logger::Info("ShaderManager", "Reloading shader...");
    
    if (!metadata.computePath.empty()) {
        // Compute shader'ı yeniden yükle
        auto computeSpirv = m_shaderCompiler->CompileShader(metadata.computePath, ShaderType::Compute);
        if (!computeSpirv.empty()) {
            shader->ReloadCompute(computeSpirv);
            metadata.lastModified = std::filesystem::last_write_time(metadata.computePath);
            Logger::Info("ShaderManager", "Compute shader reloaded successfully");
        }
    } else {
        // Vertex ve fragment shader'ı yeniden yükle
        auto vertexSpirv = m_shaderCompiler->CompileShader(metadata.vertexPath, ShaderType::Vertex);
        auto fragmentSpirv = m_shaderCompiler->CompileShader(metadata.fragmentPath, ShaderType::Fragment);
        
        if (!vertexSpirv.empty() && !fragmentSpirv.empty()) {
            shader->Reload(vertexSpirv, fragmentSpirv);
            metadata.lastModified = std::max(
                std::filesystem::last_write_time(metadata.vertexPath),
                std::filesystem::last_write_time(metadata.fragmentPath)
            );
            Logger::Info("ShaderManager", "Shader reloaded successfully");
        }
    }
}

void ShaderManager::UnloadShader(VulkanShader* shader) {
    if (!shader) {
        return;
    }
    
    // Meta veriyi bul ve sil
    auto metadataIt = m_shaderMetadata.find(shader);
    if (metadataIt != m_shaderMetadata.end()) {
        m_shaderMetadata.erase(metadataIt);
    }
    
    // Shader'ı cache'ten sil
    for (auto it = m_shaders.begin(); it != m_shaders.end(); ++it) {
        if (it->second.get() == shader) {
            m_shaders.erase(it);
            break;
        }
    }
}

void ShaderManager::CheckForShaderUpdates() {
    for (auto& [shader, metadata] : m_shaderMetadata) {
        bool needsReload = false;
        
        if (!metadata.computePath.empty()) {
            // Compute shader güncelleme kontrolü
            auto lastModified = std::filesystem::last_write_time(metadata.computePath);
            if (lastModified > metadata.lastModified) {
                needsReload = true;
            }
        } else {
            // Vertex ve fragment shader güncelleme kontrolü
            auto vertexLastModified = std::filesystem::last_write_time(metadata.vertexPath);
            auto fragmentLastModified = std::filesystem::last_write_time(metadata.fragmentPath);
            
            if (vertexLastModified > metadata.lastModified || fragmentLastModified > metadata.lastModified) {
                needsReload = true;
            }
        }
        
        if (needsReload) {
            Logger::Info("ShaderManager", "Shader update detected, reloading...");
            ReloadShader(shader);
        }
    }
}

void ShaderManager::CleanupUnusedShaders() {
    // Referans sayısı 1 olan shader'ları temizle (sadece manager'da referans var)
    for (auto it = m_shaders.begin(); it != m_shaders.end(); ) {
        if (it->second.use_count() == 1) {
            Logger::Debug("ShaderManager", "Cleaning up unused shader: {}", it->first);
            
            // Meta veriyi sil
            auto metadataIt = m_shaderMetadata.find(it->second.get());
            if (metadataIt != m_shaderMetadata.end()) {
                m_shaderMetadata.erase(metadataIt);
            }
            
            it = m_shaders.erase(it);
        } else {
            ++it;
        }
    }
}

bool ShaderManager::IsShaderLoaded(const std::string& vertexPath, const std::string& fragmentPath) const {
    std::string shaderKey = vertexPath + "|" + fragmentPath;
    return m_shaders.find(shaderKey) != m_shaders.end();
}

bool ShaderManager::IsComputeShaderLoaded(const std::string& computePath) const {
    std::string shaderKey = "compute|" + computePath;
    return m_shaders.find(shaderKey) != m_shaders.end();
}

std::vector<std::string> ShaderManager::GetLoadedShaderNames() const {
    std::vector<std::string> names;
    names.reserve(m_shaders.size());
    
    for (const auto& [key, shader] : m_shaders) {
        names.push_back(key);
    }
    
    return names;
}

size_t ShaderManager::GetLoadedShaderCount() const {
    return m_shaders.size();
}

void ShaderManager::ClearCache() {
    Logger::Info("ShaderManager", "Clearing shader cache...");
    m_shaders.clear();
    m_shaderMetadata.clear();
}

} // namespace AstralEngine
