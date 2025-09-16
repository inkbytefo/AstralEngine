#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <future>
#include <functional>

// Forward declarations
namespace AstralEngine {
class Model;
class Texture;
class VulkanShader;
class VulkanDevice;
}

namespace AstralEngine {

/**
 * @brief Oyun varlıklarını yönetir ve önbellekte tutar.
 * 
 * Async yükleme, reference counting ve otomatik bellekten çıkarma
 * özellikleri sunar. Thread-safe tasarım.
 */
class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    // Non-copyable
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // Yaşam döngüsü
    bool Initialize(const std::string& assetDirectory);
    void Update(); // Async işlemleri kontrol eder
    void Shutdown();

    // Senkron yükleme (blok eder)
    std::shared_ptr<Model> LoadModel(const std::string& filePath);
    std::shared_ptr<Texture> LoadTexture(const std::string& filePath);
    std::shared_ptr<VulkanShader> LoadShader(const std::string& vertexPath, const std::string& fragmentPath);
    std::shared_ptr<VulkanShader> LoadShader(const std::string& shaderName); // Yeni metod
    std::shared_ptr<VulkanShader> LoadShader(const std::string& shaderName, VulkanDevice* device); // VulkanRenderer için

    // Asenkron yükleme (non-blocking)
    using LoadCallback = std::function<void(std::shared_ptr<void>)>;
    void LoadModelAsync(const std::string& filePath, LoadCallback callback);
    void LoadTextureAsync(const std::string& filePath, LoadCallback callback);

    // Cache yönetimi
    void UnloadAsset(const std::string& filePath);
    void ClearCache();
    bool IsAssetLoaded(const std::string& filePath) const;
    
    // İstatistikler
    size_t GetLoadedAssetCount() const;
    size_t GetMemoryUsage() const;

    // Public cache erişim metodları
    template<typename T>
    std::shared_ptr<T> GetAssetFromCache(const std::string& filePath);

private:
    struct AssetEntry {
        std::shared_ptr<void> asset;
        std::string filePath;
        size_t refCount = 0;
        size_t memorySize = 0;
    };

    // Asset cache
    std::unordered_map<std::string, AssetEntry> m_assetCache;
    
    // Yardımcı fonksiyonlar
    std::string GetFullPath(const std::string& relativePath) const;
    std::string NormalizePath(const std::string& path) const;
    
    template<typename T>
    std::shared_ptr<T> GetFromCache(const std::string& filePath);
    
    template<typename T>
    void AddToCache(const std::string& filePath, std::shared_ptr<T> asset, size_t memorySize = 0);

    std::string m_assetDirectory;
    bool m_initialized = false;
    mutable std::mutex m_cacheMutex; // Thread safety için
};

// Template implementations
template<typename T>
std::shared_ptr<T> AssetManager::GetFromCache(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_assetCache.find(filePath);
    if (it != m_assetCache.end()) {
        return std::static_pointer_cast<T>(it->second.asset);
    }
    
    return nullptr;
}

template<typename T>
void AssetManager::AddToCache(const std::string& filePath, std::shared_ptr<T> asset, size_t memorySize) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    AssetEntry entry;
    entry.asset = std::static_pointer_cast<void>(asset);
    entry.filePath = filePath;
    entry.refCount = 1;
    entry.memorySize = memorySize;
    
    m_assetCache[filePath] = entry;
}

template<typename T>
std::shared_ptr<T> AssetManager::GetAssetFromCache(const std::string& filePath) {
    return GetFromCache<T>(filePath);
}

} // namespace AstralEngine
