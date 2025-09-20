#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <future>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
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
	void LoadAssetAsync(const AssetHandle& handle);

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
    std::vector<AssetHandle> m_loadingAssets; // Track assets currently being loaded
};


// Template implementations for GetAsset (Async version)
template<typename T>
std::shared_ptr<T> AssetManager::GetAsset(const AssetHandle& handle) {
    if (!handle.IsValid()) {
        return nullptr;
    }

    // Ensure the loading process is initiated if the asset isn't loaded or loading.
    if (m_registry.GetAssetState(handle) == AssetLoadState::NotLoaded || m_registry.GetAssetState(handle) == AssetLoadState::Unloaded) {
        RequestLoad(handle);
    }

    // Check if the asset is fully loaded to CPU.
    if (m_registry.GetAssetState(handle) != AssetLoadState::Loaded_CPU && m_registry.GetAssetState(handle) != AssetLoadState::Loaded) {
        Logger::Trace("AssetManager", "Asset {} is not ready yet (State: {}). Returning nullptr for this frame.",
                     handle.GetID(), static_cast<int>(m_registry.GetAssetState(handle)));
        return nullptr;
    }

    // If loaded, retrieve it from the cache.
    std::shared_future<std::shared_ptr<void>> future;
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_assetHandleCache.find(handle);
        if (it == m_assetHandleCache.end()) {
            Logger::Error("AssetManager", "Asset {} is marked as Loaded but not found in cache.", handle.GetID());
            return nullptr;
        }
        future = it->second;
    }

    // The future should be ready if the state is Loaded.
    if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        std::shared_ptr<void> assetData = future.get();
        if (assetData) {
            m_registry.UpdateLastAccessTime(handle);
            return std::static_pointer_cast<T>(assetData);
        }
    }
    
    // This case indicates a state mismatch, which should be logged.
    Logger::Warning("AssetManager", "Asset {} is marked as Loaded, but its future is not ready or data is null.", handle.GetID());
    return nullptr;
}

} // namespace AstralEngine
