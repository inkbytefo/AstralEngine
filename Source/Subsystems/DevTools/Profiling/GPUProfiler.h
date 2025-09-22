#pragma once
#include "ProfilingDataCollector.h"
#include <vulkan/vulkan.h>

namespace AstralEngine {

/**
 * @brief GPU profilling için uzman sınıf
 * 
 * Bu sınıf, Vulkan tabanlı GPU timing ve performans metrikleri toplama
 * işlemlerini yönetir. Timestamp query'leri kullanarak GPU üzerindeki
 * işlem sürelerini ölçer ve ProfilingDataCollector'a veri sağlar.
 * 
 * Kullanım örneği:
 * @code
 * // Frame başlangıcında
 * GPUProfiler::GetInstance().BeginFrame(commandBuffer);
 * 
 * // GPU işlemi başlangıcında
 * GPUProfiler::GetInstance().BeginScope(commandBuffer, "ShadowPass");
 * // ... GPU komutları ...
 * GPUProfiler::GetInstance().EndScope(commandBuffer);
 * 
 * // Frame sonunda
 * GPUProfiler::GetInstance().EndFrame(commandBuffer);
 * @endcode
 */
class GPUProfiler {
public:
    static GPUProfiler& GetInstance();
    
    /**
     * @brief GPU profilling sistemini başlatır
     * @param device Vulkan device
     * @param queueFamilyIndex Queue family indeksi
     */
    void Initialize(VkDevice device, uint32_t queueFamilyIndex);
    
    /**
     * @brief GPU profilling sistemini kapatır ve kaynakları temizler
     */
    void Shutdown();
    
    /**
     * @brief Yeni frame için profilling başlatır
     * @param commandBuffer Kullanılacak command buffer
     */
    void BeginFrame(VkCommandBuffer commandBuffer);
    
    /**
     * @brief Frame için profilling bitirir ve sonuçları işler
     * @param commandBuffer Kullanılacak command buffer
     */
    void EndFrame(VkCommandBuffer commandBuffer);
    
    /**
     * @brief GPU işlemi için profilling scope'u başlatır
     * @param commandBuffer Kullanılacak command buffer
     * @param name Scope adı (tanımlama için)
     */
    void BeginScope(VkCommandBuffer commandBuffer, const std::string& name);
    
    /**
     * @brief GPU işlemi için profilling scope'u bitirir
     * @param commandBuffer Kullanılacak command buffer
     */
    void EndScope(VkCommandBuffer commandBuffer);
    
    /**
     * @brief Belirtilen scope için GPU süresini döndürür
     * @param name Scope adı
     * @return GPU süresi (milisaniye cinsinden)
     */
    float GetGPUTime(const std::string& name) const;
    
    /**
     * @brief GPU profilling sisteminin başlatılıp başlatılmadığını kontrol eder
     * @return true if initialized, false otherwise
     */
    bool IsInitialized() const;
    
private:
    GPUProfiler() = default;
    ~GPUProfiler() = default;
    
    // Kopyalamayı ve taşımayı engelle
    GPUProfiler(const GPUProfiler&) = delete;
    GPUProfiler& operator=(const GPUProfiler&) = delete;
    GPUProfiler(GPUProfiler&&) = delete;
    GPUProfiler& operator=(GPUProfiler&&) = delete;
    
    struct GPUScope {
        std::string name;
        uint32_t queryIndex;
        float duration;
        
        GPUScope() : queryIndex(0), duration(0.0f) {}
    };
    
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkQueryPool m_queryPool = VK_NULL_HANDLE;
    std::vector<GPUScope> m_scopes;
    std::vector<uint32_t> m_availableQueryIndices;
    uint32_t m_currentFrameIndex = 0;
    bool m_initialized = false;
    mutable std::mutex m_mutex;
    
    /**
     * @brief Timestamp query pool oluşturur
     */
    void CreateQueryPool();
    
    /**
     * @brief Timestamp query pool'u yok eder
     */
    void DestroyQueryPool();
    
    /**
     * @brief Kullanılabilir query indeksi döndürür
     * @return Kullanılabilir query indeksi
     */
    uint32_t GetNextQueryIndex();
    
    /**
     * @brief Query indeksini kullanılabilirler listesine geri ekler
     * @param index Geri eklenecek query indeksi
     */
    void ReturnQueryIndex(uint32_t index);
    
    /**
     * @brief Query sonuçlarını işler ve süreleri hesaplar
     */
    void ProcessQueryResults();
};

} // namespace AstralEngine