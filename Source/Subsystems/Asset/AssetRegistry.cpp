#include "AssetRegistry.h"
#include "../../Core/Logger.h"
#include <chrono>
#include <algorithm>

namespace AstralEngine {

AssetRegistry::AssetRegistry() {
    Logger::Debug("AssetRegistry", "AssetRegistry created");
}

AssetRegistry::~AssetRegistry() {
    Logger::Info("AssetRegistry", "AssetRegistry destroyed. Total assets: {}", m_registry.size());
}

bool AssetRegistry::RegisterAsset(const AssetHandle& handle, const std::string& filePath, AssetHandle::Type type) {
    if (!handle.IsValid()) {
        Logger::Error("AssetRegistry", "Cannot register invalid asset handle");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Asset zaten kayıtlı mı kontrol et
    if (m_registry.find(handle) != m_registry.end()) {
        Logger::Warning("AssetRegistry", "Asset already registered: {}", filePath);
        return false;
    }
    
    // Yeni metadata oluştur
    AssetMetadata metadata;
    metadata.handle = handle;
    metadata.filePath = filePath;
    metadata.type = type;
    metadata.state = AssetLoadState::NotLoaded;
    metadata.lastAccessTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    m_registry[handle] = metadata;
    
    Logger::Debug("AssetRegistry", "Asset registered: {} (ID: {})", filePath, handle.GetID());
    return true;
}

bool AssetRegistry::UnregisterAsset(const AssetHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_registry.find(handle);
    if (it == m_registry.end()) {
        Logger::Warning("AssetRegistry", "Asset not found for unregistration: {}", handle.GetID());
        return false;
    }
    
    Logger::Debug("AssetRegistry", "Asset unregistered: {}", it->second.filePath);
    m_registry.erase(it);
    return true;
}

bool AssetRegistry::IsAssetRegistered(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.find(handle) != m_registry.end();
}

AssetMetadata* AssetRegistry::GetMetadata(const AssetHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetMetadataInternal(handle);
}

const AssetMetadata* AssetRegistry::GetMetadata(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetMetadataInternal(handle);
}

bool AssetRegistry::SetAssetState(const AssetHandle& handle, AssetLoadState state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        Logger::Error("AssetRegistry", "Cannot set state for unregistered asset: {}", handle.GetID());
        return false;
    }
    
    AssetLoadState oldState = metadata->state.load(std::memory_order_relaxed);
    metadata->state.store(state, std::memory_order_relaxed);
    
    // State değişimini logla
    const char* stateNames[] = {"NotLoaded", "Queued", "Loading", "Loaded", "Failed", "Unloaded"};
    Logger::Debug("AssetRegistry", "Asset {} state changed: {} -> {}", 
                 metadata->filePath, stateNames[static_cast<int>(oldState)], stateNames[static_cast<int>(state)]);
    
    return true;
}

AssetLoadState AssetRegistry::GetAssetState(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return AssetLoadState::NotLoaded;
    }
    
    return metadata->state.load(std::memory_order_relaxed);
}

bool AssetRegistry::SetLoadProgress(const AssetHandle& handle, float progress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return false;
    }
    
    metadata->loadProgress = std::clamp(progress, 0.0f, 1.0f);
    return true;
}

float AssetRegistry::GetLoadProgress(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return 0.0f;
    }
    
    return metadata->loadProgress;
}

uint32_t AssetRegistry::IncrementRefCount(const AssetHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return 0;
    }
    
    return ++metadata->refCount;
}

uint32_t AssetRegistry::DecrementRefCount(const AssetHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return 0;
    }
    
    if (metadata->refCount > 0) {
        --metadata->refCount;
    }
    
    return metadata->refCount;
}

uint32_t AssetRegistry::GetRefCount(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return 0;
    }
    
    return metadata->refCount;
}

bool AssetRegistry::SetAssetError(const AssetHandle& handle, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return false;
    }
    
    metadata->lastError = error;
    metadata->state = AssetLoadState::Failed;
    
    Logger::Error("AssetRegistry", "Asset error: {} - {}", metadata->filePath, error);
    return true;
}

std::string AssetRegistry::GetAssetError(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return "";
    }
    
    return metadata->lastError;
}

bool AssetRegistry::SetMemorySize(const AssetHandle& handle, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return false;
    }
    
    metadata->memorySize = size;
    return true;
}

size_t AssetRegistry::GetMemorySize(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return 0;
    }
    
    return metadata->memorySize;
}

bool AssetRegistry::SetLoadTime(const AssetHandle& handle, double timeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return false;
    }
    
    metadata->loadTimeMs = timeMs;
    return true;
}

double AssetRegistry::GetLoadTime(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return 0.0;
    }
    
    return metadata->loadTimeMs;
}

bool AssetRegistry::UpdateLastAccessTime(const AssetHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return false;
    }
    
    metadata->lastAccessTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return true;
}

uint64_t AssetRegistry::GetLastAccessTime(const AssetHandle& handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const AssetMetadata* metadata = GetMetadataInternal(handle);
    if (!metadata) {
        return 0;
    }
    
    return metadata->lastAccessTime;
}

std::vector<AssetHandle> AssetRegistry::GetAssetsByState(AssetLoadState state) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<AssetHandle> result;
    for (const auto& pair : m_registry) {
        if (pair.second.state == state) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

std::vector<AssetHandle> AssetRegistry::GetAssetsByType(AssetHandle::Type type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<AssetHandle> result;
    for (const auto& pair : m_registry) {
        if (pair.second.type == type) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

size_t AssetRegistry::GetTotalAssetCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.size();
}

size_t AssetRegistry::GetAssetCountByState(AssetLoadState state) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t count = 0;
    for (const auto& pair : m_registry) {
        if (pair.second.state == state) {
            count++;
        }
    }
    
    return count;
}

size_t AssetRegistry::GetTotalMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t totalMemory = 0;
    for (const auto& pair : m_registry) {
        totalMemory += pair.second.memorySize;
    }
    
    return totalMemory;
}

std::vector<AssetHandle> AssetRegistry::GetUnusedAssets(uint64_t olderThanMs) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::vector<AssetHandle> result;
    for (const auto& pair : m_registry) {
        if (pair.second.refCount == 0 && 
            (currentTime - pair.second.lastAccessTime) > olderThanMs) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

void AssetRegistry::ClearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Logger::Info("AssetRegistry", "Clearing all asset registrations. Count: {}", m_registry.size());
    m_registry.clear();
}

std::vector<AssetHandle> AssetRegistry::GetAllAssets() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<AssetHandle> result;
    result.reserve(m_registry.size());
    
    for (const auto& pair : m_registry) {
        result.push_back(pair.first);
    }
    
    return result;
}

// Private helper functions
AssetMetadata* AssetRegistry::GetMetadataInternal(const AssetHandle& handle) {
    auto it = m_registry.find(handle);
    return (it != m_registry.end()) ? &it->second : nullptr;
}

const AssetMetadata* AssetRegistry::GetMetadataInternal(const AssetHandle& handle) const {
    auto it = m_registry.find(handle);
    return (it != m_registry.end()) ? &it->second : nullptr;
}

} // namespace AstralEngine
