#include "VulkanMeshManager.h"
#include "../../Core/Logger.h"
#include "../../Asset/AssetData.h"

namespace AstralEngine {

VulkanMeshManager::VulkanMeshManager() {
    Logger::Debug("VulkanMeshManager", "VulkanMeshManager created");
}

VulkanMeshManager::~VulkanMeshManager() {
    Logger::Debug("VulkanMeshManager", "VulkanMeshManager destroyed");
}

bool VulkanMeshManager::Initialize(VulkanDevice* device, AssetSubsystem* assetSubsystem) {
    if (!device || !assetSubsystem) {
        SetError("Invalid parameters: device and assetSubsystem must not be null");
        return false;
    }

    m_device = device;
    m_assetSubsystem = assetSubsystem;
    m_initialized = true;

    Logger::Info("VulkanMeshManager", "VulkanMeshManager initialized successfully");
    return true;
}

void VulkanMeshManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("VulkanMeshManager", "Shutting down VulkanMeshManager...");

    // Önbelleği temizle
    ClearCache();

    m_device = nullptr;
    m_assetSubsystem = nullptr;
    m_initialized = false;

    Logger::Info("VulkanMeshManager", "VulkanMeshManager shutdown complete");
}

std::shared_ptr<VulkanMesh> VulkanMeshManager::GetOrCreateMesh(AssetHandle handle) {
    if (!m_initialized) {
        SetError("VulkanMeshManager not initialized");
        return nullptr;
    }

    if (!handle.IsValid()) {
        SetError("Invalid AssetHandle provided");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Önbellekte kontrol et
    auto it = m_meshCache.find(handle);
    if (it != m_meshCache.end()) {
        auto& entry = it->second;
        
        // Eğer mesh hazır ise döndür
        if (entry.state == GpuResourceState::Ready && entry.mesh) {
            Logger::Debug("VulkanMeshManager", "Mesh found in cache and ready for handle: {}", handle.GetID());
            return entry.mesh;
        }
        
        // Eğer mesh hala upload ediliyorsa, bu kare için atla
        if (entry.state == GpuResourceState::Uploading) {
            Logger::Trace("VulkanMeshManager", "Mesh for handle {} is still uploading. Skipping for this frame.", handle.GetID());
            return nullptr;
        }
        
        // Eğer upload başarısız olduysa, logla ve atla
        if (entry.state == GpuResourceState::Failed) {
            Logger::Warning("VulkanMeshManager", "Mesh upload failed for handle: {}. Skipping.", handle.GetID());
            return nullptr;
        }
    }

    // Önbellekte yoksa veya Unloaded durumundaysa, AssetSubsystem'den ModelData al
    auto assetManager = m_assetSubsystem->GetAssetManager();
    if (!assetManager) {
        SetError("AssetManager not available from AssetSubsystem");
        return nullptr;
    }

    // ModelData'yı asenkron olarak al
    auto modelData = assetManager->GetAsset<ModelData>(handle);

    // Eğer ModelData henüz hazır değilse, bu kare için atla
    if (!modelData) {
        Logger::Trace("VulkanMeshManager", "ModelData for handle {} is not yet loaded. Skipping mesh creation for this frame.", handle.GetID());
        return nullptr;
    }

    // ModelData hazır ama geçersiz mi kontrol et
    if (!modelData->IsValid()) {
        SetError("ModelData for handle " + std::to_string(handle.GetID()) + " is loaded but invalid.");
        Logger::Warning("VulkanMeshManager", "ModelData for handle {} is invalid.", handle.GetID());
        return nullptr;
    }

    // Yeni mesh oluştur (asenkron upload için)
    auto mesh = CreateMeshFromData(modelData, handle);
    if (!mesh) {
        Logger::Error("VulkanMeshManager", "Failed to create mesh from ModelData for handle: {}", handle.GetID());
        return nullptr;
    }

    // Yeni cache entry oluştur ve Uploading olarak işaretle
    MeshCacheEntry entry;
    entry.mesh = mesh;
    entry.state = GpuResourceState::Uploading;
    entry.needsCompletion = true; // CompleteImageInitialization benzeri bir işlem gerektiğini belirt
    
    // Önbelleğe ekle
    m_meshCache[handle] = entry;
    Logger::Info("VulkanMeshManager", "Created mesh and marked as uploading for handle: {} (vertices: {}, indices: {})",
                 handle.GetID(), modelData->GetVertexCount(), modelData->GetIndexCount());

    // Mesh hala upload edildiği için nullptr döndür, CheckUploadCompletions hazır olunca dönecek
    return nullptr;
}

void VulkanMeshManager::RemoveMesh(AssetHandle handle) {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_meshCache.find(handle);
    if (it != m_meshCache.end()) {
        Logger::Debug("VulkanMeshManager", "Removing mesh from cache for handle: {}", handle.GetID());
        m_meshCache.erase(it);
    }
}

void VulkanMeshManager::ClearCache() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    Logger::Info("VulkanMeshManager", "Clearing mesh cache ({} meshes)", m_meshCache.size());
    
    // Tüm mesh'leri ve staging buffer'ları temizle
    for (auto& pair : m_meshCache) {
        auto& entry = pair.second;
        
        // Mesh'i shutdown et
        if (entry.mesh) {
            entry.mesh->Shutdown();
        }
        
        // Staging buffer'ları temizle
        if (entry.stagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->GetDevice(), entry.stagingBuffer, nullptr);
        }
        if (entry.stagingMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->GetDevice(), entry.stagingMemory, nullptr);
        }
        
        // Fence'i temizle
        if (entry.uploadFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device->GetDevice(), entry.uploadFence, nullptr);
        }
    }

    m_meshCache.clear();
}

bool VulkanMeshManager::HasMesh(AssetHandle handle) const {
    if (!m_initialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_meshCache.find(handle) != m_meshCache.end();
}

std::vector<AssetHandle> VulkanMeshManager::GetCachedHandles() const {
    std::vector<AssetHandle> handles;
    
    if (!m_initialized) {
        return handles;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    handles.reserve(m_meshCache.size());
    for (const auto& pair : m_meshCache) {
        handles.push_back(pair.first);
    }
    
    return handles;
}

std::shared_ptr<VulkanMesh> VulkanMeshManager::CreateMeshFromData(const std::shared_ptr<ModelData>& modelData, AssetHandle handle) {
    if (!modelData || !modelData->IsValid()) {
        SetError("Invalid ModelData provided");
        return nullptr;
    }

    // Yeni VulkanMesh oluştur
    auto mesh = std::make_shared<VulkanMesh>();
    if (!mesh) {
        SetError("Failed to allocate VulkanMesh");
        return nullptr;
    }

    // Mesh'i vertex ve index verileriyle başlat
    if (!mesh->Initialize(m_device, modelData->vertices, modelData->indices, modelData->boundingBox)) {
        SetError("Failed to initialize VulkanMesh: " + mesh->GetLastError());
        return nullptr;
    }

    Logger::Debug("VulkanMeshManager", "Successfully created mesh from ModelData for handle: {}", handle.GetID());
    return mesh;
}

void VulkanMeshManager::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("VulkanMeshManager", "{}", error);
}

void VulkanMeshManager::CheckUploadCompletions() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    std::vector<AssetHandle> completedHandles;
    
    // Tüm Uploading durumundaki mesh'leri kontrol et
    for (auto& pair : m_meshCache) {
        auto& entry = pair.second;
        
        if (entry.state == GpuResourceState::Uploading) {
            // Mesh'in durumunu kontrol et
            if (entry.mesh && entry.mesh->IsReady()) {
                // Mesh hazır, staging kaynaklarını temizle
                entry.mesh->CleanupStagingResources();
                
                // Durumu Ready olarak güncelle
                entry.state = GpuResourceState::Ready;
                entry.needsCompletion = false;
                completedHandles.push_back(pair.first);
                
                Logger::Info("VulkanMeshManager", "Mesh upload completed for handle: {}", pair.first.GetID());
                
            } else if (entry.mesh && entry.mesh->GetState() == GpuResourceState::Failed) {
                // Yükleme başarısız oldu
                Logger::Error("VulkanMeshManager", "Mesh upload failed for handle: {}", pair.first.GetID());
                entry.state = GpuResourceState::Failed;
                entry.needsCompletion = false;
                
                // Kaynakları temizle
                if (entry.mesh) {
                    entry.mesh->CleanupStagingResources();
                }
                
            } else {
                // Henüz tamamlanmadı, devam et
                Logger::Trace("VulkanMeshManager", "Mesh upload still in progress for handle: {}", pair.first.GetID());
            }
        }
    }
    
    if (!completedHandles.empty()) {
        Logger::Info("VulkanMeshManager", "Completed {} mesh uploads this frame", completedHandles.size());
    }
}

GpuResourceState VulkanMeshManager::GetMeshState(AssetHandle handle) const {
    if (!m_initialized) {
        return GpuResourceState::Unloaded;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_meshCache.find(handle);
    if (it != m_meshCache.end()) {
        return it->second.state;
    }
    
    return GpuResourceState::Unloaded;
}

bool VulkanMeshManager::IsMeshReady(AssetHandle handle) const {
    if (!m_initialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_meshCache.find(handle);
    if (it != m_meshCache.end()) {
        const auto& entry = it->second;
        return entry.state == GpuResourceState::Ready && entry.mesh != nullptr;
    }
    
    return false;
}

void VulkanMeshManager::CleanupMeshResources(AssetHandle handle) {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_meshCache.find(handle);
    if (it != m_meshCache.end()) {
        auto& entry = it->second;
        
        Logger::Info("VulkanMeshManager", "Cleaning up mesh resources for handle: {}", handle.GetID());
        
        // Mesh'i shutdown et
        if (entry.mesh) {
            entry.mesh->Shutdown();
            entry.mesh.reset();
        }
        
        // Staging buffer'ları temizle
        if (entry.stagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->GetDevice(), entry.stagingBuffer, nullptr);
            entry.stagingBuffer = VK_NULL_HANDLE;
        }
        if (entry.stagingMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->GetDevice(), entry.stagingMemory, nullptr);
            entry.stagingMemory = VK_NULL_HANDLE;
        }
        
        // Fence'i temizle
        if (entry.uploadFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device->GetDevice(), entry.uploadFence, nullptr);
            entry.uploadFence = VK_NULL_HANDLE;
        }
        
        // Durumu güncelle
        entry.state = GpuResourceState::Unloaded;
        entry.needsCompletion = false;
        
        // Cache'den kaldır
        m_meshCache.erase(it);
        
        Logger::Debug("VulkanMeshManager", "Mesh resources cleaned up for handle: {}", handle.GetID());
    }
}

} // namespace AstralEngine
