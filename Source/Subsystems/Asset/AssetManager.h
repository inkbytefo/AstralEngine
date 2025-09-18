#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <future>
#include <functional>
#include "AssetHandle.h"
#include "AssetRegistry.h"
#include "AssetData.h"
#include "../../Core/Logger.h"
#include "../../Core/ThreadPool.h" // Added for ThreadPool

// Forward declarations
namespace AstralEngine {
class Texture {
public:
    Texture(const std::string& path) : m_filePath(path) {}
    const std::string& GetFilePath() const { return m_filePath; }
private:
    std::string m_filePath;
};

class Model;
class VulkanDevice;
}

namespace AstralEngine {

/**
 * @brief Manages game assets using a handle-based asynchronous architecture.
 * 
 * Provides asynchronous asset loading, reference counting, and automatic memory management.
 * The system uses AssetHandle for asset identification and supports non-blocking
 * asset retrieval through GetAsset<T>(). Thread-safe design.
 */
class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    // Non-copyable
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // Lifecycle management
    bool Initialize(const std::string& assetDirectory);
    void Update(); // Controls async operations
    void Shutdown();

    // CPU-side data loading methods (synchronous, used by async system)
    std::shared_ptr<ModelData> LoadModel(const std::string& filePath);
    std::shared_ptr<TextureData> LoadTexture(const std::string& filePath);
    std::shared_ptr<ShaderData> LoadShader(const std::string& filePath, ShaderData::Type type = ShaderData::Type::Unknown);
    std::shared_ptr<MaterialData> LoadMaterial(const std::string& filePath);
    
    // AssetHandle-based System
    AssetHandle RegisterAsset(const std::string& filePath, AssetHandle::Type type);
    bool UnloadAsset(const AssetHandle& handle);
    
    // Asset Access
    template<typename T>
    std::shared_ptr<T> GetAsset(const AssetHandle& handle);
    
    template<typename T>
    bool IsAssetLoaded(const AssetHandle& handle) const;
    
    // Asset State Management
    AssetLoadState GetAssetState(const AssetHandle& handle) const;
    float GetAssetLoadProgress(const AssetHandle& handle) const;
    std::string GetAssetError(const AssetHandle& handle) const;
    
    // Asset Registry Access
    AssetRegistry& GetRegistry() { return m_registry; }
    const AssetRegistry& GetRegistry() const { return m_registry; }

    // Asynchronous Loading (non-blocking)
    using LoadCallback = std::function<void(std::shared_ptr<void>)>;
    void LoadModelAsync(const std::string& filePath, LoadCallback callback);
    void LoadTextureAsync(const std::string& filePath, LoadCallback callback);

    // Cache Management
    void ClearAssetCache();
    size_t GetLoadedAssetCountByType(AssetHandle::Type type) const;
    
    // Statistics
    size_t GetLoadedAssetCount() const;
    size_t GetMemoryUsage() const;

private:
    // Handle-based asset cache for asynchronous loading
    std::unordered_map<AssetHandle, std::shared_future<std::shared_ptr<void>>, AssetHandleHash> m_assetHandleCache;
    
    // Asset registry
    AssetRegistry m_registry;

    // Thread pool for async loading
    std::unique_ptr<ThreadPool> m_threadPool;
    
    // Helper functions
    std::string GetFullPath(const std::string& relativePath) const;
    std::string NormalizePath(const std::string& path) const;

    // Asynchronous loading request
    void RequestLoad(const AssetHandle& handle);

    std::string m_assetDirectory;
    bool m_initialized = false;
    mutable std::mutex m_cacheMutex; // Thread safety
};


// Template implementations for GetAsset (Async version)
template<typename T>
std::shared_ptr<T> AssetManager::GetAsset(const AssetHandle& handle) {
    if (!handle.IsValid()) {
        return nullptr;
    }

    // Initiate loading process (if not already started)
    // This is always a non-blocking call
    RequestLoad(handle);

    std::shared_future<std::shared_ptr<void>> future;
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_assetHandleCache.find(handle);
        if (it == m_assetHandleCache.end()) {
            // Future should not be missing immediately after RequestLoad
            // This indicates an error condition
            Logger::Error("AssetManager", "Asset future not found in cache after RequestLoad: {}", handle.GetID());
            return nullptr;
        }
        future = it->second;
    }

    // Check future status non-blocking
    if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        // Asset is loaded, get the data and return it
        // future.get() can be called multiple times for shared_future
        std::shared_ptr<void> assetData = future.get();
        
        if (assetData) {
            // Update AssetRegistry (success status, last access time, etc.)
            // This is done in the main thread (render thread), not worker thread
            m_registry.SetAssetState(handle, AssetLoadState::Loaded);
            m_registry.SetLoadProgress(handle, 1.0f);
            m_registry.UpdateLastAccessTime(handle);
            
            // Memory size is already set in RequestLoad using assetData->GetMemoryUsage()

            Logger::Trace("AssetManager", "Asset {} is ready, returning data.", handle.GetID());
            return std::static_pointer_cast<T>(assetData);
        } else {
            // Future is ready but data is nullptr, meaning loading failed
            Logger::Warning("AssetManager", "Asset {} loading resulted in null data (failed).", handle.GetID());
            m_registry.SetAssetState(handle, AssetLoadState::Failed);
            m_registry.SetAssetError(handle, "Async loading resulted in null data.");
            return nullptr;
        }
    } else {
        // Asset is not yet loaded, skip for this frame
        // AssetRegistry status is already set to "Loading"
        Logger::Trace("AssetManager", "Asset {} is still loading, skipping for this frame.", handle.GetID());
        return nullptr;
    }
}

} // namespace AstralEngine
