#pragma once

#include "AssetHandle.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>

namespace AstralEngine {

/**
 * @brief Asset yükleme durumları
 */
enum class AssetLoadState {
    NotLoaded,      // Henüz yüklenmedi
    Queued,         // Yükleme kuyruğunda bekliyor
    Loading,        // Yükleniyor
    Loaded_CPU,     // CPU belleğine yüklendi, GPU için hazır
    Loaded,         // Yüklendi ve kullanıma hazır (legacy, Loaded_CPU ile aynı)
    Failed,         // Yükleme başarısız oldu
    Unloaded        // Bellekten çıkarıldı
};

/**
 * @brief Asset metadata bilgileri
 */
struct AssetMetadata {
    AssetHandle handle;
    std::string filePath;
    AssetHandle::Type type;
    std::atomic<AssetLoadState> state;
    size_t memorySize;
    uint32_t refCount;
    std::string lastError;
    float loadProgress;  // 0.0f - 1.0f
    
    // Performance metrics
    double loadTimeMs;
    uint64_t lastAccessTime;
    
    AssetMetadata() 
        : state(AssetLoadState::NotLoaded)
        , memorySize(0)
        , refCount(0)
        , loadProgress(0.0f)
        , loadTimeMs(0.0)
        , lastAccessTime(0) {
    }
    
    // Move constructor
    AssetMetadata(AssetMetadata&& other) noexcept
        : handle(std::move(other.handle))
        , filePath(std::move(other.filePath))
        , type(other.type)
        , state(other.state.load(std::memory_order_relaxed))
        , memorySize(other.memorySize)
        , refCount(other.refCount)
        , lastError(std::move(other.lastError))
        , loadProgress(other.loadProgress)
        , loadTimeMs(other.loadTimeMs)
        , lastAccessTime(other.lastAccessTime) {
    }
    
    // Move assignment operator
    AssetMetadata& operator=(AssetMetadata&& other) noexcept {
        if (this != &other) {
            handle = std::move(other.handle);
            filePath = std::move(other.filePath);
            type = other.type;
            state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
            memorySize = other.memorySize;
            refCount = other.refCount;
            lastError = std::move(other.lastError);
            loadProgress = other.loadProgress;
            loadTimeMs = other.loadTimeMs;
            lastAccessTime = other.lastAccessTime;
        }
        return *this;
    }
    
    // Non-copyable
    AssetMetadata(const AssetMetadata&) = delete;
    AssetMetadata& operator=(const AssetMetadata&) = delete;
};

/**
 * @brief Tüm asset'lerin kayıtını tutan registry
 * 
 * Asset metadata, yükleme durumları ve referans sayılarını yönetir.
 * Thread-safe tasarım.
 */
class AssetRegistry {
public:
    AssetRegistry();
    ~AssetRegistry();

    // Non-copyable
    AssetRegistry(const AssetRegistry&) = delete;
    AssetRegistry& operator=(const AssetRegistry&) = delete;

    // Asset kayıt yönetimi
    bool RegisterAsset(const AssetHandle& handle, const std::string& filePath, AssetHandle::Type type);
    bool UnregisterAsset(const AssetHandle& handle);
    bool IsAssetRegistered(const AssetHandle& handle) const;

    // Metadata erişimi
    AssetMetadata* GetMetadata(const AssetHandle& handle);
    const AssetMetadata* GetMetadata(const AssetHandle& handle) const;
    
    // State yönetimi
    bool SetAssetState(const AssetHandle& handle, AssetLoadState state);
    AssetLoadState GetAssetState(const AssetHandle& handle) const;
    
    // Progress tracking
    bool SetLoadProgress(const AssetHandle& handle, float progress);
    float GetLoadProgress(const AssetHandle& handle) const;
    
    // Reference counting
    uint32_t IncrementRefCount(const AssetHandle& handle);
    uint32_t DecrementRefCount(const AssetHandle& handle);
    uint32_t GetRefCount(const AssetHandle& handle) const;
    
    // Error handling
    bool SetAssetError(const AssetHandle& handle, const std::string& error);
    std::string GetAssetError(const AssetHandle& handle) const;
    
    // Memory management
    bool SetMemorySize(const AssetHandle& handle, size_t size);
    size_t GetMemorySize(const AssetHandle& handle) const;
    
    // Performance metrics
    bool SetLoadTime(const AssetHandle& handle, double timeMs);
    double GetLoadTime(const AssetHandle& handle) const;
    bool UpdateLastAccessTime(const AssetHandle& handle);
    uint64_t GetLastAccessTime(const AssetHandle& handle) const;
    
    // Sorgular ve istatistikler
    std::vector<AssetHandle> GetAssetsByState(AssetLoadState state) const;
    std::vector<AssetHandle> GetAssetsByType(AssetHandle::Type type) const;
    size_t GetTotalAssetCount() const;
    size_t GetAssetCountByState(AssetLoadState state) const;
    size_t GetTotalMemoryUsage() const;
    
    // Cleanup operations
    std::vector<AssetHandle> GetUnusedAssets(uint64_t olderThanMs = 0) const;
    void ClearAll();
    
    // Get all assets
    std::vector<AssetHandle> GetAllAssets() const;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<AssetHandle, AssetMetadata, AssetHandleHash> m_registry;
    
    // Helper functions
    AssetMetadata* GetMetadataInternal(const AssetHandle& handle);
    const AssetMetadata* GetMetadataInternal(const AssetHandle& handle) const;
};

} // namespace AstralEngine
