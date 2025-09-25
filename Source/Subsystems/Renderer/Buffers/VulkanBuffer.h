#pragma once

#include "../Core/VulkanDevice.h"
#include <vulkan/vulkan.h>
#include <cstdint>

// Forward declaration to break circular dependency
class GraphicsDevice;

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
 * @brief Vulkan buffer yönetimi için RAII tabanlı sınıf.
 * 
 * Bu sınıf, VkBuffer ve VkDeviceMemory nesnelerini birlikte yönetir.
 * Vertex buffer, index buffer, uniform buffer gibi farklı türlerde
 * buffer'lar oluşturmak için kullanılır. Veri transferleri için
 * GraphicsDevice'daki merkezi VulkanTransferManager'ı kullanır.
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
        std::string name = "UnnamedBuffer";    ///< Buffer adı (debug için)
    };

    VulkanBuffer();
    ~VulkanBuffer();

    // Non-copyable
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    /**
     * @brief Buffer'ı başlatır.
     * @param graphicsDevice Geçerli GraphicsDevice.
     * @param config Buffer yapılandırma ayarları.
     * @return Başarılı olursa true.
     */
    bool Initialize(GraphicsDevice* graphicsDevice, const Config& config);
    
    /**
     * @brief Buffer kaynaklarını serbest bırakır.
     * 
     * Kaynaklar anında serbest bırakılmaz, GraphicsDevice'ın
     * frame-aware silme kuyruğuna eklenir.
     */
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

    /**
     * @brief Host'tan (CPU) gelen veriyi bu buffer'a (GPU) kopyalar.
     *
     * Bu metod, merkezi transfer yöneticisini kullanarak veriyi asenkron olarak
     * GPU'ya transfer eder. İşlem, transfer yöneticisinin bir sonraki
     * SubmitTransfers() çağrısında işlenir.
     *
     * @param data Kopyalanacak verinin başlangıç adresi.
     * @param dataSize Kopyalanacak verinin boyutu.
     */
    void CopyDataFromHost(const void* data, VkDeviceSize dataSize);

    /**
     * @brief Descriptor set'ler için buffer bilgisi döndürür.
     * 
     * Bu metod, uniform buffer'ların descriptor set'lerde kullanılması için
     * gerekli VkDescriptorBufferInfo yapısını oluşturur.
     * 
     * @return VkDescriptorBufferInfo Buffer descriptor bilgisi
     */
    VkDescriptorBufferInfo GetDescriptorInfo() const {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = m_size;
        return bufferInfo;
    }

private:
    // Yardımcı metotlar
    void SetError(const std::string& error);

    // Member değişkenler
    GraphicsDevice* m_graphicsDevice = nullptr;
    VulkanDevice* m_device = nullptr; // Cached from GraphicsDevice
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    VkBufferUsageFlags m_usage = 0;
    VkMemoryPropertyFlags m_properties = 0;
    void* m_mappedData = nullptr;
    std::string m_lastError;
    bool m_isInitialized = false;
    bool m_mapped = false;
    
    mutable GpuResourceState m_state = GpuResourceState::Unloaded;
};

} // namespace AstralEngine
