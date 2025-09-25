#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <future>
#include <mutex>
#include "AssetHandle.h"
#include "AssetRegistry.h"
#include "IAssetImporter.h"
#include "AssetData.h"
#include "../../Core/ThreadPool.h"
#include "../../Core/Logger.h"

namespace AstralEngine {

/**
 * @brief Manages game assets using a handle-based asynchronous architecture.
 *
 * This class is the central point for asset operations. It uses a system of
 * importers (IAssetImporter) to load assets asynchronously from disk.
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
    void Shutdown();

    // Modern Asset Loading API
    template<typename T>
    AssetHandle Load(const std::string& filePath) {
        static_assert(std::is_base_of_v<ModelData, T> ||
                     std::is_base_of_v<TextureData, T> ||
                     std::is_base_of_v<ShaderData, T> ||
                     std::is_base_of_v<MaterialData, T>,
                     "T must be a valid asset data type (ModelData, TextureData, ShaderData, or MaterialData)");
        AssetHandle handle = RegisterAsset(filePath);
        if (handle.IsValid()) {
            GetAsset<T>(handle); // Trigger async loading
        }
        return handle;
    }

    template<typename T>
    std::future<AssetHandle> LoadAsync(const std::string& filePath) {
        static_assert(std::is_base_of_v<ModelData, T> ||
                     std::is_base_of_v<TextureData, T> ||
                     std::is_base_of_v<ShaderData, T> ||
                     std::is_base_of_v<MaterialData, T>,
                     "T must be a valid asset data type (ModelData, TextureData, ShaderData, or MaterialData)");
        return m_threadPool->Submit([this, filePath]() {
            return Load<T>(filePath);
        });
    }

    // Legacy Asset Registration (for backwards compatibility)
    AssetHandle RegisterAsset(const std::string& filePath);
    bool UnloadAsset(const AssetHandle& handle);

    // Asynchronous Asset Access
    template<typename T>
    std::shared_ptr<T> GetAsset(const AssetHandle& handle);

    // Asset State & Info
    bool IsAssetLoaded(const AssetHandle& handle) const;
    AssetLoadState GetAssetState(const AssetHandle& handle) const;
    const AssetMetadata* GetMetadata(const AssetHandle& handle) const;

    // Asset Registry Access
    AssetRegistry& GetRegistry() { return m_registry; }
    const AssetRegistry& GetRegistry() const { return m_registry; }

    // Utility
    std::string GetFullPath(const std::string& relativePath) const;

    // Update and monitoring
    void Update();
    void CheckForAssetChanges();

private:
    void RegisterImporters();
    AssetHandle::Type GetAssetTypeFromFileExtension(const std::string& filePath) const;
    void LoadAssetAsync(const AssetHandle& handle);

    template<typename T>
    void RegisterImporter(AssetHandle::Type type);

private:
    std::string m_assetDirectory;
    bool m_initialized = false;

    AssetRegistry m_registry;
    std::unique_ptr<ThreadPool> m_threadPool;

    // Caches for loaded assets and in-flight promises
    mutable std::mutex m_cacheMutex;
    std::unordered_map<AssetHandle, std::shared_future<std::shared_ptr<void>>, AssetHandleHash> m_assetCache;

    // Importer registration
    std::unordered_map<AssetHandle::Type, std::unique_ptr<IAssetImporter>> m_importers;
};

// Template Implementations

template<typename T>
std::shared_ptr<T> AssetManager::GetAsset(const AssetHandle& handle) {
    if (!handle.IsValid()) {
        return nullptr;
    }

    const AssetMetadata* metadata = m_registry.GetMetadata(handle);
    if (!metadata) {
        Logger::Warning("AssetManager", "Attempted to get asset with unregistered handle: {}", handle.GetID());
        return nullptr;
    }

    // If asset is not loaded, not queued, and not currently loading, start the loading process.
    AssetLoadState currentState = metadata->state.load();
    if (currentState == AssetLoadState::NotLoaded || currentState == AssetLoadState::Unloaded) {
        LoadAssetAsync(handle);
        // Return nullptr for now, the asset will be available in a future frame.
        return nullptr;
    }

    // If the asset is still in the process of loading, return nullptr.
    if (currentState == AssetLoadState::Queued || currentState == AssetLoadState::Loading) {
        return nullptr;
    }

    // If loading failed, don't attempt to retrieve it.
    if (currentState == AssetLoadState::Failed) {
        return nullptr;
    }

    // At this point, the asset should be in the cache.
    std::shared_future<std::shared_ptr<void>> future;
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_assetCache.find(handle);
        if (it == m_assetCache.end()) {
            // This can happen if the asset was just loaded but the cache hasn't been populated yet for this thread.
            // Or it indicates a logic error.
            Logger::Warning("AssetManager", "Asset {} is marked as loaded but not found in cache. It might become available next frame.", handle.GetID());
            return nullptr;
        }
        future = it->second;
    }

    // The future should be ready. A wait of 0 seconds is a non-blocking check.
    if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        std::shared_ptr<void> assetData = future.get();
        if (assetData) {
            m_registry.UpdateLastAccessTime(handle);
            return std::static_pointer_cast<T>(assetData);
        }
    }

    return nullptr; // Should not be reached in normal operation if state is Loaded.
}

template<typename T>
void AssetManager::RegisterImporter(AssetHandle::Type type) {
    static_assert(std::is_base_of<IAssetImporter, T>::value, "Importer type must derive from IAssetImporter");
    if (m_importers.find(type) != m_importers.end()) {
        Logger::Warning("AssetManager", "Importer for type {} is already registered.", (int)type);
        return;
    }
    m_importers[type] = std::make_unique<T>();
    Logger::Debug("AssetManager", "Registered importer for asset type {}", (int)type);
}

} // namespace AstralEngine
