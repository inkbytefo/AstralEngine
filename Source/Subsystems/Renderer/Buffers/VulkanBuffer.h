#pragma once

#include "../Core/VulkanDevice.h"
#include <vulkan/vulkan.h>
#include <cstdint>

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
};

} // namespace AstralEngine
