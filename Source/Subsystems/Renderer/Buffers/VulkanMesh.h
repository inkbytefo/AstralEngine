#pragma once

#include "../Core/VulkanDevice.h"
#include "VulkanBuffer.h"
#include "../RendererTypes.h"
#include "../VulkanMeshManager.h"
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
     * @return true Başarılı olursa
     * @return false Hata oluşursa
     */
    bool Initialize(VulkanDevice* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

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
     * @brief Buffer'a veri kopyalamak için staging buffer kullanır
     * 
     * @param dstBuffer Hedef buffer
     * @param bufferSize Buffer boyutu
     * @param data Kopyalanacak veri
     * @return true Başarılı olursa
     * @return false Hata oluşursa
     */
    bool CopyDataToBuffer(VulkanBuffer* dstBuffer, VkDeviceSize bufferSize, const void* data);

    /**
     * @brief Hata mesajını ayarlar
     *
     * @param error Hata mesajı
     */
    void SetError(const std::string& error);

    /**
     * @brief Staging buffer ve fence kaynaklarını temizler
     *
     * Bu metod, asenkron upload işlemi tamamlandığında staging buffer
     * ve fence kaynaklarını temizlemek için kullanılır.
     */
    void CleanupStagingResources();

    // Member değişkenler
    std::vector<Vertex> m_vertices;                    // Vertex verileri listesi
    std::vector<uint32_t> m_indices;                   // Index verileri listesi
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;      // Vertex buffer için
    std::unique_ptr<VulkanBuffer> m_indexBuffer;       // Index buffer için
    VulkanDevice* m_device;                            // Vulkan cihaz referansı
    std::string m_lastError;                           // Son hata mesajı
    bool m_isInitialized;                              // Başlatma durumu
    
    // Asenkron upload için üye değişkenler
    VkFence m_uploadFence;                             // Upload işlemini senkronize etmek için fence
    VkBuffer m_stagingBuffer;                          // Geçici staging buffer
    VkDeviceMemory m_stagingMemory;                    // Staging buffer için bellek
    GpuResourceState m_state;                          // Mesh'in yükleme durumu
};

} // namespace AstralEngine
