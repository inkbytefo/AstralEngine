#pragma once

#include "VulkanUtils.h"
#include "Core/VulkanDevice.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>

namespace AstralEngine {

/**
 * @enum SemaphoreType
 * @brief Semaphore tipleri
 */
enum class SemaphoreType {
    Binary,      ///< Binary semaphore (0 veya 1)
    Timeline     ///< Timeline semaphore (sayaçlı)
};

/**
 * @struct SemaphoreCreateInfo
 * @brief Semaphore oluşturma bilgileri
 */
struct SemaphoreCreateInfo {
    SemaphoreType type = SemaphoreType::Binary;  ///< Semaphore tipi
    uint64_t initialValue = 0;                   ///< Timeline semaphore için başlangıç değeri
    std::string debugName = "";                  ///< Debug için isim
};

/**
 * @class VulkanSemaphore
 * @brief Modern Vulkan semaphore sarmalayıcısı
 * 
 * Bu sınıf, VK_KHR_synchronization2 extension'ını kullanarak
 * modern semaphore yönetimi sağlar. Hem binary hem de
 * timeline semaphore'ları destekler.
 */
class VulkanSemaphore {
public:
    VulkanSemaphore();
    ~VulkanSemaphore();

    // Non-copyable
    VulkanSemaphore(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

    // Movable
    VulkanSemaphore(VulkanSemaphore&& other) noexcept;
    VulkanSemaphore& operator=(VulkanSemaphore&& other) noexcept;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const SemaphoreCreateInfo& createInfo = SemaphoreCreateInfo{});
    void Shutdown();

    // Timeline semaphore işlemleri
    bool Signal(uint64_t value = 1);
    bool Wait(uint64_t value, uint64_t timeout = std::numeric_limits<uint64_t>::max());
    uint64_t GetCurrentValue() const;

    // Binary semaphore işlemleri
    bool SignalBinary();
    bool WaitBinary(uint64_t timeout = std::numeric_limits<uint64_t>::max());

    // Getter'lar
    VkSemaphore GetSemaphore() const { return m_semaphore; }
    SemaphoreType GetType() const { return m_type; }
    bool IsInitialized() const { return m_initialized; }
    const std::string& GetDebugName() const { return m_debugName; }

    // VK_KHR_synchronization2 için submit info
    VkSemaphoreSubmitInfoKHR GetSubmitInfo(VkPipelineStageFlags2 stageMask, uint64_t value = 0) const;

private:
    // Timeline semaphore için özel fonksiyonlar
    bool InitializeTimelineSemaphore(VkDevice device, uint64_t initialValue);
    bool InitializeBinarySemaphore(VkDevice device);

    // Hata yönetimi
    void SetError(const std::string& error);

    // Member değişkenler
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    VulkanDevice* m_device = nullptr;
    SemaphoreType m_type = SemaphoreType::Binary;
    std::string m_debugName;
    std::string m_lastError;
    bool m_initialized = false;
};

/**
 * @struct FenceCreateInfo
 * @brief Fence oluşturma bilgileri
 */
struct FenceCreateInfo {
    bool signaled = false;                     ///< Başlangıçta sinyallenmiş mi
    std::string debugName = "";                  ///< Debug için isim
};

/**
 * @class VulkanFence
 * @brief Modern Vulkan fence sarmalayıcısı
 * 
 * Bu sınıf, VK_KHR_synchronization2 extension'ını kullanarak
 * modern fence yönetimi sağlar. CPU-GPU senkronizasyonu için kullanılır.
 */
class VulkanFence {
public:
    VulkanFence();
    ~VulkanFence();

    // Non-copyable
    VulkanFence(const VulkanFence&) = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;

    // Movable
    VulkanFence(VulkanFence&& other) noexcept;
    VulkanFence& operator=(VulkanFence&& other) noexcept;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const FenceCreateInfo& createInfo = FenceCreateInfo{});
    void Shutdown();

    // Fence işlemleri
    bool Reset();
    bool Wait(uint64_t timeout = std::numeric_limits<uint64_t>::max());
    bool IsSignaled() const;

    // Getter'lar
    VkFence GetFence() const { return m_fence; }
    bool IsInitialized() const { return m_initialized; }
    const std::string& GetDebugName() const { return m_debugName; }

private:
    // Hata yönetimi
    void SetError(const std::string& error);

    // Member değişkenler
    VkFence m_fence = VK_NULL_HANDLE;
    VulkanDevice* m_device = nullptr;
    std::string m_debugName;
    std::string m_lastError;
    bool m_initialized = false;
};

/**
 * @struct BarrierInfo
 * @brief Memory barrier bilgileri
 */
struct BarrierInfo {
    VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_NONE; ///< Kaynak aşama
    VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_NONE; ///< Hedef aşama
    VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE;             ///< Kaynak erişim
    VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE;             ///< Hedef erişim
    VkDependencyFlags dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT; ///< Bağımlılık bayrakları
    VkBuffer buffer = VK_NULL_HANDLE;                              ///< Buffer (isteğe bağlı)
    VkImage image = VK_NULL_HANDLE;                                ///< Image (isteğe bağlı)
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;          ///< Eski layout (image için)
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;          ///< Yeni layout (image için)
    VkImageSubresourceRange subresourceRange{};                   ///< Image alt kaynak aralığı
    std::string debugName = "";                                   ///< Debug için isim
};

/**
 * @class VulkanSynchronization
 * @brief Modern Vulkan senkronizasyon yöneticisi
 * 
 * Bu sınıf, VK_KHR_synchronization2 extension'ını kullanarak
 * modern senkronizasyon yönetimi sağlar. Semaphore'lar, fence'ler
 * ve memory barrier'lar için merkezi bir arayüz sunar.
 */
class VulkanSynchronization {
public:
    /**
     * @struct Config
     * @brief Senkronizasyon yöneticisi yapılandırması
     */
    struct Config {
        bool enableTimelineSemaphores = true;      ///< Timeline semaphore desteği
        bool enableDebugNames = true;              ///< Debug isimleri
        uint32_t maxSemaphores = 32;               ///< Maksimum semaphore sayısı
        uint32_t maxFences = 16;                   ///< Maksimum fence sayısı
        uint64_t defaultTimeout = 1000000000;      ///< Varsayılan timeout (1 saniye)
        
        Config() : enableTimelineSemaphores(true),
                  enableDebugNames(true),
                  maxSemaphores(32),
                  maxFences(16),
                  defaultTimeout(1000000000) {}
    };

    VulkanSynchronization();
    ~VulkanSynchronization();

    // Non-copyable
    VulkanSynchronization(const VulkanSynchronization&) = delete;
    VulkanSynchronization& operator=(const VulkanSynchronization&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const Config& config = Config{});
    void Shutdown();

    // Semaphore yönetimi
    std::unique_ptr<VulkanSemaphore> CreateSemaphore(const SemaphoreCreateInfo& createInfo = SemaphoreCreateInfo{});
    void DestroySemaphore(std::unique_ptr<VulkanSemaphore> semaphore);

    // Fence yönetimi
    std::unique_ptr<VulkanFence> CreateFence(const FenceCreateInfo& createInfo = FenceCreateInfo{});
    void DestroyFence(std::unique_ptr<VulkanFence> fence);

    // Memory barrier yönetimi
    VkMemoryBarrier2 CreateMemoryBarrier(const BarrierInfo& info) const;
    VkBufferMemoryBarrier2 CreateBufferMemoryBarrier(const BarrierInfo& info) const;
    VkImageMemoryBarrier2 CreateImageMemoryBarrier(const BarrierInfo& info) const;

    // Command buffer için senkronizasyon
    void PipelineBarrier(
        VkCommandBuffer commandBuffer,
        const std::vector<VkMemoryBarrier2>& memoryBarriers,
        const std::vector<VkBufferMemoryBarrier2>& bufferBarriers,
        const std::vector<VkImageMemoryBarrier2>& imageBarriers,
        VkDependencyFlags dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    );

    // Submit işlemleri için yardımcı fonksiyonlar
    VkSubmitInfo2 CreateSubmitInfo(
        const std::vector<VkCommandBufferSubmitInfo>& commandBufferInfos,
        const std::vector<VkSemaphoreSubmitInfo>& waitSemaphoreInfos,
        const std::vector<VkSemaphoreSubmitInfo>& signalSemaphoreInfos
    );

    bool QueueSubmit2(
        VkQueue queue,
        const VkSubmitInfo2& submitInfo,
        VkFence fence = VK_NULL_HANDLE
    );

    // Timeline semaphore senkronizasyonu
    bool WaitSemaphores(
        const std::vector<VkSemaphore>& semaphores,
        const std::vector<uint64_t>& values,
        uint64_t timeout = std::numeric_limits<uint64_t>::max()
    );

    bool SignalSemaphores(
        const std::vector<VkSemaphore>& semaphores,
        const std::vector<uint64_t>& values
    );

    // Getter'lar
    VulkanDevice* GetDevice() const { return m_device; }
    const Config& GetConfig() const { return m_config; }
    bool IsInitialized() const { return m_initialized; }
    bool IsTimelineSemaphoreSupported() const { return m_timelineSemaphoreSupported; }

    // İstatistikler
    struct Statistics {
        uint32_t semaphoreCount = 0;           ///< Toplam semaphore sayısı
        uint32_t fenceCount = 0;               ///< Toplam fence sayısı
        uint32_t barrierCount = 0;              ///< Toplam barrier sayısı
        uint32_t submitCount = 0;               ///< Toplam submit sayısı
        uint64_t totalWaitTime = 0;            ///< Toplam bekleme süresi (nanosaniye)
        uint64_t totalSignalTime = 0;           ///< Toplam sinyal süresi (nanosaniye)
    };

    Statistics GetStatistics() const;
    std::string GetDebugReport() const;

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Yardımcı metodlar
    bool CheckTimelineSemaphoreSupport();
    bool ValidateConfig(const Config& config);
    
    // Debug ve loglama
    void LogSemaphoreCreation(const VulkanSemaphore& semaphore) const;
    void LogFenceCreation(const VulkanFence& fence) const;
    void LogBarrierCreation(const BarrierInfo& info) const;
    void LogSubmit(const VkSubmitInfo2& submitInfo) const;
    
    // Hata yönetimi
    void SetError(const std::string& error);
    void ClearError();
    
    // Zaman ölçümü için yardımcı fonksiyonlar
    uint64_t GetTimeNanoseconds() const;
    void UpdateWaitTime(uint64_t duration);
    void UpdateSignalTime(uint64_t duration);

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    Config m_config;
    std::string m_lastError;
    bool m_initialized = false;
    bool m_timelineSemaphoreSupported = false;
    
    // İstatistikler
    mutable Statistics m_statistics;
    
    // Atomic sayaçlar
    mutable std::atomic<uint32_t> m_semaphoreCount{0};
    mutable std::atomic<uint32_t> m_fenceCount{0};
    mutable std::atomic<uint32_t> m_barrierCount{0};
    mutable std::atomic<uint32_t> m_submitCount{0};
    mutable std::atomic<uint64_t> m_totalWaitTime{0};
    mutable std::atomic<uint64_t> m_totalSignalTime{0};
};

/**
 * @brief Senkronizasyon için yardımcı fonksiyonlar
 */
namespace SyncUtils {
    /**
     * @brief Pipeline stage flag'lerini string'e dönüştür
     */
    std::string PipelineStageFlagsToString(VkPipelineStageFlags2 flags);
    
    /**
     * @brief Access flag'leri string'e dönüştür
     */
    std::string AccessFlagsToString(VkAccessFlags2 flags);
    
    /**
     * @brief Image layout'ı string'e dönüştür
     */
    std::string ImageLayoutToString(VkImageLayout layout);
    
    /**
     * @brief Dependency flag'leri string'e dönüştür
     */
    std::string DependencyFlagsToString(VkDependencyFlags flags);
    
    /**
     * @brief Submit info'yi debug için formatla
     */
    std::string FormatSubmitInfo(const VkSubmitInfo2& submitInfo);
    
    /**
     * @brief Barrier info'yi debug için formatla
     */
    std::string FormatBarrierInfo(const BarrierInfo& info);
    
    /**
     * @brief Zamanı okunabilir formata dönüştür
     */
    std::string FormatTimeNanoseconds(uint64_t nanoseconds);
}

/**
 * @brief RAII için senkronizasyon deleter'ları
 */
struct SemaphoreDeleter {
    VulkanSynchronization* manager = nullptr;
    
    SemaphoreDeleter(VulkanSynchronization* mgr = nullptr) : manager(mgr) {}
    
    void operator()(VulkanSemaphore* semaphore) const {
        if (manager && semaphore) {
            std::unique_ptr<VulkanSemaphore> ptr(semaphore);
            manager->DestroySemaphore(std::move(ptr));
        } else {
            delete semaphore;
        }
    }
};

struct FenceDeleter {
    VulkanSynchronization* manager = nullptr;
    
    FenceDeleter(VulkanSynchronization* mgr = nullptr) : manager(mgr) {}
    
    void operator()(VulkanFence* fence) const {
        if (manager && fence) {
            std::unique_ptr<VulkanFence> ptr(fence);
            manager->DestroyFence(std::move(ptr));
        } else {
            delete fence;
        }
    }
};

// RAII için smart pointer typedef'leri
using VulkanSemaphorePtr = std::unique_ptr<VulkanSemaphore, SemaphoreDeleter>;
using VulkanFencePtr = std::unique_ptr<VulkanFence, FenceDeleter>;

} // namespace AstralEngine
