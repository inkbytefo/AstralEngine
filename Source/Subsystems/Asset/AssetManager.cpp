#include "AssetManager.h"
#include "../../Core/Logger.h"
#include <filesystem>
#include <mutex>

// Forward declarations for asset types (will be implemented later)
namespace AstralEngine {
class Model {
public:
    Model(const std::string& path) : m_filePath(path) {}
    const std::string& GetFilePath() const { return m_filePath; }
private:
    std::string m_filePath;
};

class Texture {
public:
    Texture(const std::string& path) : m_filePath(path) {}
    const std::string& GetFilePath() const { return m_filePath; }
private:
    std::string m_filePath;
};

class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath) 
        : m_vertexPath(vertPath), m_fragmentPath(fragPath) {}
    const std::string& GetVertexPath() const { return m_vertexPath; }
    const std::string& GetFragmentPath() const { return m_fragmentPath; }
private:
    std::string m_vertexPath;
    std::string m_fragmentPath;
};
}

namespace AstralEngine {

AssetManager::AssetManager() {
    Logger::Debug("AssetManager", "AssetManager created");
}

AssetManager::~AssetManager() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("AssetManager", "AssetManager destroyed");
}

bool AssetManager::Initialize(const std::string& assetDirectory) {
    if (m_initialized) {
        Logger::Warning("AssetManager", "AssetManager already initialized");
        return true;
    }
    
    m_assetDirectory = assetDirectory;
    
    // Asset dizininin var olup olmadığını kontrol et
    if (!std::filesystem::exists(assetDirectory)) {
        Logger::Error("AssetManager", "Asset directory does not exist: '{}'", assetDirectory);
        return false;
    }
    
    if (!std::filesystem::is_directory(assetDirectory)) {
        Logger::Error("AssetManager", "Asset path is not a directory: '{}'", assetDirectory);
        return false;
    }
    
    m_initialized = true;
    Logger::Info("AssetManager", "AssetManager initialized with directory: '{}'", assetDirectory);
    
    return true;
}

void AssetManager::Update() {
    // TODO: Async yükleme operasyonlarını kontrol et
    // Future'ları check et, tamamlananları callback'leri çağır
}

void AssetManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("AssetManager", "Shutting down AssetManager. Loaded assets: {}, Memory usage: {} bytes", 
                GetLoadedAssetCount(), GetMemoryUsage());
    
    ClearCache();
    m_initialized = false;
    
    Logger::Info("AssetManager", "AssetManager shutdown complete");
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& filePath) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot load model: AssetManager not initialized");
        return nullptr;
    }
    
    std::string normalizedPath = NormalizePath(filePath);
    
    // Cache'den kontrol et
    auto cached = GetFromCache<Model>(normalizedPath);
    if (cached) {
        Logger::Debug("AssetManager", "Model loaded from cache: '{}'", filePath);
        return cached;
    }
    
    // Dosyadan yükle
    std::string fullPath = GetFullPath(normalizedPath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Model file not found: '{}'", fullPath);
        return nullptr;
    }
    
    // TODO: Gerçek model yükleme implementasyonu (tinyobjloader, Assimp vs.)
    auto model = std::make_shared<Model>(fullPath);
    
    // Cache'e ekle
    size_t estimatedSize = std::filesystem::file_size(fullPath);
    AddToCache(normalizedPath, model, estimatedSize);
    
    Logger::Info("AssetManager", "Model loaded successfully: '{}'", filePath);
    return model;
}

std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& filePath) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot load texture: AssetManager not initialized");
        return nullptr;
    }
    
    std::string normalizedPath = NormalizePath(filePath);
    
    // Cache'den kontrol et
    auto cached = GetFromCache<Texture>(normalizedPath);
    if (cached) {
        Logger::Debug("AssetManager", "Texture loaded from cache: '{}'", filePath);
        return cached;
    }
    
    // Dosyadan yükle
    std::string fullPath = GetFullPath(normalizedPath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Texture file not found: '{}'", fullPath);
        return nullptr;
    }
    
    // TODO: Gerçek texture yükleme implementasyonu (stb_image, FreeImage vs.)
    auto texture = std::make_shared<Texture>(fullPath);
    
    // Cache'e ekle
    size_t estimatedSize = std::filesystem::file_size(fullPath);
    AddToCache(normalizedPath, texture, estimatedSize);
    
    Logger::Info("AssetManager", "Texture loaded successfully: '{}'", filePath);
    return texture;
}

std::shared_ptr<Shader> AssetManager::LoadShader(const std::string& vertexPath, const std::string& fragmentPath) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot load shader: AssetManager not initialized");
        return nullptr;
    }
    
    std::string normalizedVertexPath = NormalizePath(vertexPath);
    std::string normalizedFragmentPath = NormalizePath(fragmentPath);
    
    // Shader için unique key oluştur
    std::string shaderKey = normalizedVertexPath + "|" + normalizedFragmentPath;
    
    // Cache'den kontrol et
    auto cached = GetFromCache<Shader>(shaderKey);
    if (cached) {
        Logger::Debug("AssetManager", "Shader loaded from cache: '{}' + '{}'", vertexPath, fragmentPath);
        return cached;
    }
    
    // Dosyalardan yükle
    std::string fullVertexPath = GetFullPath(normalizedVertexPath);
    std::string fullFragmentPath = GetFullPath(normalizedFragmentPath);
    
    if (!std::filesystem::exists(fullVertexPath)) {
        Logger::Error("AssetManager", "Vertex shader file not found: '{}'", fullVertexPath);
        return nullptr;
    }
    
    if (!std::filesystem::exists(fullFragmentPath)) {
        Logger::Error("AssetManager", "Fragment shader file not found: '{}'", fullFragmentPath);
        return nullptr;
    }
    
    // TODO: Gerçek shader yükleme implementasyonu
    auto shader = std::make_shared<Shader>(fullVertexPath, fullFragmentPath);
    
    // Cache'e ekle
    size_t estimatedSize = std::filesystem::file_size(fullVertexPath) + 
                          std::filesystem::file_size(fullFragmentPath);
    AddToCache(shaderKey, shader, estimatedSize);
    
    Logger::Info("AssetManager", "Shader loaded successfully: '{}' + '{}'", vertexPath, fragmentPath);
    return shader;
}

void AssetManager::LoadModelAsync(const std::string& filePath, LoadCallback callback) {
    (void)callback; // Suppress unused parameter warning
    // TODO: Thread pool ile async yükleme implementasyonu
    Logger::Warning("AssetManager", "Async model loading not implemented yet: '{}'", filePath);
}

void AssetManager::LoadTextureAsync(const std::string& filePath, LoadCallback callback) {
    (void)callback; // Suppress unused parameter warning
    // TODO: Thread pool ile async yükleme implementasyonu
    Logger::Warning("AssetManager", "Async texture loading not implemented yet: '{}'", filePath);
}

void AssetManager::UnloadAsset(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_assetCache.find(filePath);
    if (it != m_assetCache.end()) {
        Logger::Debug("AssetManager", "Unloading asset: '{}'", filePath);
        m_assetCache.erase(it);
    }
}

void AssetManager::ClearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    Logger::Info("AssetManager", "Clearing asset cache. {} assets removed", m_assetCache.size());
    m_assetCache.clear();
}

bool AssetManager::IsAssetLoaded(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    return m_assetCache.find(filePath) != m_assetCache.end();
}

size_t AssetManager::GetLoadedAssetCount() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    return m_assetCache.size();
}

size_t AssetManager::GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    size_t totalMemory = 0;
    for (const auto& pair : m_assetCache) {
        totalMemory += pair.second.memorySize;
    }
    
    return totalMemory;
}

std::string AssetManager::GetFullPath(const std::string& relativePath) const {
    return m_assetDirectory + "/" + relativePath;
}

std::string AssetManager::NormalizePath(const std::string& path) const {
    // Path normalize işlemleri (slash/backslash dönüşümü vs.)
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

} // namespace AstralEngine
