#pragma once

#include "../Core/VulkanDevice.h"
#include <vulkan/vulkan.h>
#include <cstdint>

// GPU kaynak durumu enum'u
enum class GpuResourceState {
    Unloaded,   ///< Henüz yüklenmeye başlanmadı
    Uploading,  ///< GPU'ya upload ediliyor
    Ready,      ///< GPU'da kullanıma hazır
    Failed      ///< Yükleme başarısız oldu
};

namespace AstralEngine {

/**
 * @class VulkanBuffer
 * @brief Vulkan buffer yönetimi için sınıf
 * 
 * Bu sınıf, VkBuffer ve VkDeviceMemory nesnelerini birlikte yönetir.
 * Vertex buffer, index buffer, staging buffer gibi farklı türlerde
 * buffer'lar oluşturmak için kullanılabilir.
 */
class VulkanBuffer {
public:
    /**
     * @brief Buffer yapılandırma parametreleri
     */
    struct Config {
        VkDeviceSize size;                    ///< Buffer boyutu
        VkBufferUsageFlags usage;              ///< Buffer kullanım amacı
        VkMemoryPropertyFlags properties;      ///< Bellek özellikleri
    };

    VulkanBuffer();
    ~VulkanBuffer();

    // Non-copyable
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const Config& config);
    void Shutdown();

    // Bellek eşleme (mapping)
    void* Map();
    void Unmap();

    // Getter'lar
    VkBuffer GetBuffer() const { return m_buffer; }
    VkDeviceMemory GetMemory() const { return m_bufferMemory; }
    VkDeviceSize GetSize() const { return m_size; }
    bool IsInitialized() const { return m_isInitialized; }
    bool IsMapped() const { return m_mapped; }

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

    // Sahipliği bırakmak için
    void Release();

    // Staging buffer ve veri kopyalama metotları
    /**
     * @brief Host'tan gelen veriyi buffer'a kopyalar
     *
     * Bu metod, staging buffer oluşturur, veriyi staging buffer'a kopyalar ve
     * asenkron olarak GPU'ya transfer eder. async parametresi true ise fence döndürür,
     * false ise senkron olarak bekler.
     *
     * @param device Vulkan device
     * @param data Kopyalanacak veri
     * @param dataSize Veri boyutu
     * @param async Asenkron çalıştır (varsayılan: true)
     * @return VkFence Asenkron ise fence handle, senkron ise VK_NULL_HANDLE
     */
    VkFence CopyDataFromHost(const void* data, VkDeviceSize dataSize, bool async = true);
    
    /**
     * @brief Staging buffer kaynaklarını temizler
     *
     * Bu metod, asenkron upload işlemi tamamlandığında staging buffer
     * ve fence kaynaklarını temizlemek için kullanılır.
     */
    void CleanupStagingResources();
    
    /**
     * @brief Upload işleminin tamamlanıp tamamlanmadığını kontrol eder
     *
     * @return true Upload tamamlandıysa
     * @return false Upload hala devam ediyorsa
     */
    bool IsUploadComplete() const;

private:
    // Yardımcı metotlar
    void SetError(const std::string& error);

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    VkBufferUsageFlags m_usage = 0;
    VkMemoryPropertyFlags m_properties = 0;
    void* m_mappedData = nullptr;
    std::string m_lastError;
    bool m_isInitialized = false;
    bool m_mapped = false;
    
    // Asenkron upload için üye değişkenler
    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;        // Geçici staging buffer
    VkDeviceMemory m_stagingMemory = VK_NULL_HANDLE;  // Staging buffer için bellek
    VkFence m_uploadFence = VK_NULL_HANDLE;           // Upload işlemini senkronize etmek için fence
    mutable GpuResourceState m_state = GpuResourceState::Unloaded; // Buffer'ın yükleme durumu
    mutable bool m_autoCleanupStaging = true;         // Otomatik staging temizliği etkin mi?
};

} // namespace AstralEngine
