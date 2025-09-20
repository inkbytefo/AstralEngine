#pragma once

#include "../../../Subsystems/Asset/AssetHandle.h"
#include "../../../Subsystems/Asset/AssetSubsystem.h"
#include "../Buffers/VulkanTexture.h"
#include "../Core/VulkanDevice.h"
#include "RendererTypes.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vulkan/vulkan.h>

namespace AstralEngine {

/**
 * @brief Texture cache entry'si için veri yapısı
 */
struct TextureCacheEntry {
    std::shared_ptr<VulkanTexture> texture;
    GpuResourceState state = GpuResourceState::Unloaded;
    VkFence uploadFence = VK_NULL_HANDLE;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
};

/**
 * @class VulkanTextureManager
 * @brief GPU-side texture kaynaklarını yöneten ve önbellekte tutan sınıf
 * 
 * Bu sınıf, AssetSubsystem'den gelen TextureData verilerini kullanarak
 * VulkanTexture nesneleri oluşturur ve önbellekte tutar. Aynı texture için
 * tekrar tekrar GPU kaynakları oluşturulmasını engeller.
 * 
 * @author inkbytefo
 * @date 2025
 */
class VulkanTextureManager {
public:
    VulkanTextureManager();
    ~VulkanTextureManager();

    // Non-copyable
    VulkanTextureManager(const VulkanTextureManager&) = delete;
    VulkanTextureManager& operator=(const VulkanTextureManager&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, AssetSubsystem* assetSubsystem);
    void Shutdown();

    /**
     * @brief AssetHandle için texture kaynağını al veya oluştur
     *
     * Bu metod, verilen AssetHandle için önbellekte bir VulkanTexture varsa onu döndürür.
     * Eğer yoksa, AssetSubsystem'den TextureData alır, yeni bir VulkanTexture oluşturur
     * ve önbelleğe ekler.
     *
     * @param handle Texture asset handle'ı
     * @return std::shared_ptr<VulkanTexture> Oluşturulan veya önbellekten alınan texture, hazır değilse nullptr
     */
    std::shared_ptr<VulkanTexture> GetOrCreateTexture(AssetHandle handle);
    
    /**
     * @brief AssetHandle için texture kaynağını al (sadece hazır olanlar)
     *
     * Bu metod, verilen AssetHandle için önbellekte hazır bir VulkanTexture varsa onu döndürür.
     * Eğer texture hazır değilse veya önbellekte yoksa nullptr döndürür.
     *
     * @param handle Texture asset handle'ı
     * @return std::shared_ptr<VulkanTexture> Önbellekten alınan hazır texture, hazır değilse nullptr
     */
    std::shared_ptr<VulkanTexture> GetTexture(AssetHandle handle);

    /**
     * @brief Önbellekten bir texture'ı kaldır
     * 
     * @param handle Kaldırılacak texture'ın asset handle'ı
     */
    void RemoveTexture(AssetHandle handle);

    /**
     * @brief Tüm önbelleği temizle
     */
    void ClearCache();

    // İstatistikler
    size_t GetCachedTextureCount() const { return m_textureCache.size(); }
    bool HasTexture(AssetHandle handle) const;
    std::vector<AssetHandle> GetCachedHandles() const;

    // Asenkron upload yönetimi
    void CheckUploadCompletions();

    // Texture durum sorgulama metodları
    /**
     * @brief Texture'ın yükleme durumunu döndürür
     *
     * @param handle Texture asset handle'ı
     * @return GpuResourceState Texture'ın yükleme durumu
     */
    GpuResourceState GetTextureState(AssetHandle handle) const;
    
    /**
     * @brief Texture'ın hazır olup olmadığını kontrol eder
     *
     * @param handle Texture asset handle'ı
     * @return true Hazırsa
     * @return false Hazır değilse veya önbellekte yoksa
     */
    bool IsTextureReady(AssetHandle handle) const;
    
    /**
     * @brief Belirli bir texture'ın kaynaklarını temizler
     *
     * @param handle Temizlenecek texture'ın asset handle'ı
     */
    void CleanupTextureResources(AssetHandle handle);

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

private:
    /**
     * @brief TextureData'dan yeni bir VulkanTexture oluştur
     * 
     * @param textureData Texture verisi
     * @param handle Asset handle (loglama için)
     * @return std::shared_ptr<VulkanTexture> Oluşturulan texture
     */
    std::shared_ptr<VulkanTexture> CreateTextureFromData(const std::shared_ptr<TextureData>& textureData, AssetHandle handle);

    /**
     * @brief TextureData'dan VkFormat belirle
     * 
     * @param textureData Texture verisi
     * @return VkFormat Vulkan formatı
     */
    VkFormat DetermineVkFormat(const std::shared_ptr<TextureData>& textureData) const;

    /**
     * @brief Hata mesajını ayarla
     * 
     * @param error Hata mesajı
     */
    void SetError(const std::string& error);

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;
    
    // Texture önbelleği
    std::unordered_map<AssetHandle, TextureCacheEntry, AssetHandleHash> m_textureCache;
    
    // Thread safety
    mutable std::mutex m_cacheMutex;
    
    // Durum
    bool m_initialized = false;
    std::string m_lastError;
};

} // namespace AstralEngine
