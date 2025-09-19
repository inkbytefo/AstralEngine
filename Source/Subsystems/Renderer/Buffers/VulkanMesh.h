#pragma once

#include "../Core/VulkanDevice.h"
#include "VulkanBuffer.h"
#include "../RendererTypes.h"
#include "../VulkanMeshManager.h"
#include "../Bounds.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <cstdint>

namespace AstralEngine {

/**
 * @class VulkanMesh
 * @brief Bir modelin tek bir çizilebilir parçasını temsil eden sınıf
 * 
 * Bu sınıf, vertex ve index verilerini GPU belleğinde yönetir ve
 * çizim işlemleri için gerekli buffer'ları sağlar. Her mesh,
 * bir modelin tek bir çizilebilir parçasını temsil eder (örneğin bir karakter,
 * bir obje, bir terrain parçası vb.).
 * 
 * @author inkbytefo
 * @date 2025
 */
class VulkanMesh {
public:
    VulkanMesh();
    ~VulkanMesh();

    // Non-copyable
    VulkanMesh(const VulkanMesh&) = delete;
    VulkanMesh& operator=(const VulkanMesh&) = delete;

    // Movable
    VulkanMesh(VulkanMesh&&) = default;
    VulkanMesh& operator=(VulkanMesh&&) = default;

    /**
     * @brief Mesh'i vertex ve index verileriyle başlatır
     * 
     * Bu metod, gelen vertex ve index verilerini GPU'ya yüklemek için
     * gerekli buffer'ları oluşturur ve veriyi kopyalar.
     * 
     * @param device Vulkan cihazı
     * @param vertices Vertex verileri listesi
     * @param indices Index verileri listesi
     * @param boundingBox Mesh'in sınırlayıcı kutusu
     * @return true Başarılı olursa
     * @return false Hata oluşursa
     */
    bool Initialize(VulkanDevice* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const AABB& boundingBox);

    /**
     * @brief Mesh kaynaklarını temizler
     */
    void Shutdown();

    /**
     * @brief Vertex ve index buffer'larını command buffer'a bağlar
     * 
     * Bu metod, vkCmdBindVertexBuffers ve vkCmdBindIndexBuffer
     * komutlarını kullanarak buffer'ları command buffer'a bağlar.
     * 
     * @param commandBuffer Komut buffer'ı
     */
    void Bind(VkCommandBuffer commandBuffer) const;

    /**
     * @brief Index sayısını döndürür
     * 
     * @return uint32_t Index sayısı
     */
    uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_indices.size()); }

    /**
     * @brief Sınırlayıcı kutuyu döndürür
     * 
     * @return const AABB& Sınırlayıcı kutu referansı
     */
    const AABB& GetBoundingBox() const { return m_boundingBox; }

    /**
     * @brief Vertex sayısını döndürür
     * 
     * @return uint32_t Vertex sayısı
     */
    uint32_t GetVertexCount() const { return static_cast<uint32_t>(m_vertices.size()); }

    /**
     * @brief Mesh'in başlatılıp başlatılmadığını kontrol eder
     * 
     * @return true Başlatıldıysa
     * @return false Başlatılmadıysa
     */
    bool IsInitialized() const { return m_isInitialized; }

    /**
     * @brief Mesh'in GPU'da kullanıma hazır olup olmadığını kontrol eder
     *
     * @return true Hazırsa (fence sinyal vermişse)
     * @return false Henüz hazır değilse
     */
    bool IsReady() const;

    /**
     * @brief Mesh'in yükleme durumunu döndürür
     *
     * @return GpuResourceState Mesh'in yükleme durumu
     */
    GpuResourceState GetState() const { return m_state; }

    /**
     * @brief Son hata mesajını döndürür
     *
     * @return const std::string& Hata mesajı
     */
    const std::string& GetLastError() const { return m_lastError; }

private:
    /**
     * @brief Vertex buffer'ı oluşturur ve veriyi yükler
     * 
     * @param vertices Vertex verileri
     * @return true Başarılı olursa
     * @return false Hata oluşursa
     */
    bool CreateVertexBuffer(const std::vector<Vertex>& vertices);

    /**
     * @brief Index buffer'ı oluşturur ve veriyi yükler
     * 
     * @param indices Index verileri
     * @return true Başarılı olursa
     * @return false Hata oluşursa
     */
    bool CreateIndexBuffer(const std::vector<uint32_t>& indices);

    /**
     * @brief Vertex ve index verilerini GPU'ya yükler
     *
     * Bu metod, hem vertex hem de index verilerini tek bir staging buffer kullanarak
     * GPU'ya asenkron olarak yükler.
     *
     * @return true Başarılı olursa
     * @return false Hata oluşursa
     */
    bool UploadGpuData();

    /**
     * @brief Hata mesajını ayarlar
     *
     * @param error Hata mesajı
     */
    void SetError(const std::string& error);

    // Member değişkenler
    AABB m_boundingBox;                                // Mesh'in sınırlayıcı kutusu
    std::vector<Vertex> m_vertices;                    // Vertex verileri listesi
    std::vector<uint32_t> m_indices;                   // Index verileri listesi
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;      // Vertex buffer için
    std::unique_ptr<VulkanBuffer> m_indexBuffer;       // Index buffer için
    VulkanDevice* m_device;                            // Vulkan cihaz referansı
    std::string m_lastError;                           // Son hata mesajı
    bool m_isInitialized;                              // Başlatma durumu
    mutable GpuResourceState m_state;                  // Mesh'in yükleme durumu
    VkFence m_uploadFence = VK_NULL_HANDLE;            // Upload fence takibi
    VkBuffer m_stagingBuffer = VK_NULL_HANDLE;         // Staging buffer
    VkDeviceMemory m_stagingMemory = VK_NULL_HANDLE;   // Staging buffer için bellek
};

} // namespace AstralEngine
