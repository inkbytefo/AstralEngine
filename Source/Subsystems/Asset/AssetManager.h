#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <future>
#include <functional>
#include "Model.h"
#include "AssetHandle.h"
#include "AssetRegistry.h"
#include "ShaderProgram.h"
#include "../../Core/Logger.h"

// Forward declarations
namespace AstralEngine {
class Texture {
public:
    Texture(const std::string& path) : m_filePath(path) {}
    const std::string& GetFilePath() const { return m_filePath; }
private:
    std::string m_filePath;
};

class VulkanShader;
class VulkanDevice;
class VulkanTexture;
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

    // Senkron yükleme (blok eder) - Legacy support
    std::shared_ptr<Model> LoadModel(const std::string& filePath);
    std::shared_ptr<Model> LoadModel(const std::string& filePath, VulkanDevice* device);
    std::shared_ptr<Texture> LoadTexture(const std::string& filePath);
    std::shared_ptr<VulkanTexture> LoadVulkanTexture(const std::string& filePath, VulkanDevice* device);
    
    // Birleştirilmiş shader loading API - tek kaynak olarak bu metod kullanılacak
    std::shared_ptr<ShaderProgram> LoadShader(const std::string& shaderName, VulkanDevice* device);

    // Yeni AssetHandle tabanlı sistem
    AssetHandle RegisterAsset(const std::string& filePath, AssetHandle::Type type);
    bool LoadAsset(const AssetHandle& handle, VulkanDevice* device = nullptr);
    bool UnloadAsset(const AssetHandle& handle);
    
    // Asset erişim metodları
    template<typename T>
    std::shared_ptr<T> GetAsset(const AssetHandle& handle);
    
    template<typename T>
    bool IsAssetLoaded(const AssetHandle& handle) const;
    
    // Asset durum yönetimi
    AssetLoadState GetAssetState(const AssetHandle& handle) const;
    float GetAssetLoadProgress(const AssetHandle& handle) const;
    std::string GetAssetError(const AssetHandle& handle) const;
    
    // Asset registry erişimi
    AssetRegistry& GetRegistry() { return m_registry; }
    const AssetRegistry& GetRegistry() const { return m_registry; }

    // Asenkron yükleme (non-blocking)
    using LoadCallback = std::function<void(std::shared_ptr<void>)>;
    void LoadModelAsync(const std::string& filePath, LoadCallback callback);
    void LoadTextureAsync(const std::string& filePath, LoadCallback callback);

    // Legacy cache yönetimi (string tabanlı)
    void UnloadAsset(const std::string& filePath);
    void ClearCache();
    bool IsAssetLoaded(const std::string& filePath) const;
    
    // Yeni cache yönetimi (Handle tabanlı)
    void ClearAssetCache();
    size_t GetLoadedAssetCountByType(AssetHandle::Type type) const;
    
    // İstatistikler
    size_t GetLoadedAssetCount() const;
    size_t GetMemoryUsage() const;

    // Public cache erişim metodları
    template<typename T>
    std::shared_ptr<T> GetAssetFromCache(const std::string& filePath); // Legacy
    
    template<typename T>
    std::shared_ptr<T> GetAssetFromCache(const AssetHandle& handle); // Yeni

private:
    struct AssetEntry {
        std::shared_ptr<void> asset;
        std::string filePath;
        size_t refCount = 0;
        size_t memorySize = 0;
    };

    // Legacy asset cache (string tabanlı - geriye uyumluluk için)
    std::unordered_map<std::string, AssetEntry> m_assetCache;
    
    // Yeni asset cache (Handle tabanlı)
    std::unordered_map<AssetHandle, std::shared_ptr<void>, AssetHandleHash> m_assetHandleCache;
    
    // Asset registry
    AssetRegistry m_registry;
    
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

// Template implementations for GetAsset
template<typename T>
std::shared_ptr<T> AssetManager::GetAsset(const AssetHandle& handle) {
    if (!handle.IsValid()) {
        return nullptr;
    }
    
    // Registry'den asset durumunu kontrol et
    AssetLoadState state = m_registry.GetAssetState(handle);
    if (state != AssetLoadState::Loaded) {
        Logger::Debug("AssetManager", "Asset not loaded: {} (state: {})", 
                     handle.GetID(), static_cast<int>(state));
        return nullptr;
    }
    
    // Cache'den asset'i al
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_assetHandleCache.find(handle);
        if (it != m_assetHandleCache.end()) {
            m_registry.UpdateLastAccessTime(handle);
            return std::static_pointer_cast<T>(it->second);
        }
    }
    
    Logger::Warning("AssetManager", "Asset marked as loaded but not found in cache: {}", handle.GetID());
    return nullptr;
}

} // namespace AstralEngine
