#pragma once

#include "../Asset/AssetHandle.h"
#include "../Asset/AssetSubsystem.h"
#include "../Core/VulkanDevice.h"
#include "../RendererTypes.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vulkan/vulkan.h>

namespace AstralEngine {

// Forward declarations
class VulkanMesh;

/**
 * @brief Mesh cache entry'si için veri yapısı
 */
struct MeshCacheEntry {
    std::shared_ptr<VulkanMesh> mesh;
    GpuResourceState state = GpuResourceState::Unloaded;
    VkFence uploadFence = VK_NULL_HANDLE;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    bool needsCompletion = false; ///< CompleteImageInitialization benzeri bir işlem gerekip gerekmediğini belirtir
};

/**
 * @class VulkanMeshManager
 * @brief GPU-side mesh kaynaklarını yöneten ve önbellekte tutan sınıf
 * 
 * Bu sınıf, AssetSubsystem'den gelen ModelData verilerini kullanarak
 * VulkanMesh nesneleri oluşturur ve önbellekte tutar. Aynı model için
 * tekrar tekrar GPU kaynakları oluşturulmasını engeller.
 * 
 * @author inkbytefo
 * @date 2025
 */
class VulkanMeshManager {
public:
    VulkanMeshManager();
    ~VulkanMeshManager();

    // Non-copyable
    VulkanMeshManager(const VulkanMeshManager&) = delete;
    VulkanMeshManager& operator=(const VulkanMeshManager&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, AssetSubsystem* assetSubsystem);
    void Shutdown();

    /**
     * @brief AssetHandle için mesh kaynağını al veya oluştur
     * 
     * Bu metod, verilen AssetHandle için önbellekte bir VulkanMesh varsa onu döndürür.
     * Eğer yoksa, AssetSubsystem'den ModelData alır, yeni bir VulkanMesh oluşturur
     * ve önbelleğe ekler.
     * 
     * @param handle Mesh asset handle'ı
     * @return std::shared_ptr<VulkanMesh> Oluşturulan veya önbellekten alınan mesh
     */
    std::shared_ptr<VulkanMesh> GetOrCreateMesh(AssetHandle handle);

    /**
     * @brief Önbellekten bir mesh'i kaldır
     * 
     * @param handle Kaldırılacak mesh'in asset handle'ı
     */
    void RemoveMesh(AssetHandle handle);

    /**
     * @brief Tüm önbelleği temizle
     */
    void ClearCache();

    // İstatistikler
    size_t GetCachedMeshCount() const { return m_meshCache.size(); }
    bool HasMesh(AssetHandle handle) const;
    std::vector<AssetHandle> GetCachedHandles() const;

    // Asenkron upload yönetimi
    void CheckUploadCompletions();

    // Mesh durum sorgulama metodları
    GpuResourceState GetMeshState(AssetHandle handle) const;
    bool IsMeshReady(AssetHandle handle) const;
    void CleanupMeshResources(AssetHandle handle);

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

private:
    /**
     * @brief ModelData'dan yeni bir VulkanMesh oluştur
     * 
     * @param modelData Model verisi
     * @param handle Asset handle (loglama için)
     * @return std::shared_ptr<VulkanMesh> Oluşturulan mesh
     */
    std::shared_ptr<VulkanMesh> CreateMeshFromData(const std::shared_ptr<ModelData>& modelData, AssetHandle handle);

    /**
     * @brief Hata mesajını ayarla
     * 
     * @param error Hata mesajı
     */
    void SetError(const std::string& error);

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;
    
    // Mesh önbelleği
    std::unordered_map<AssetHandle, MeshCacheEntry, AssetHandleHash> m_meshCache;
    
    // Thread safety
    mutable std::mutex m_cacheMutex;
    
    // Durum
    bool m_initialized = false;
    std::string m_lastError;
};

} // namespace AstralEngine
