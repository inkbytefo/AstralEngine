#pragma once

#include "VulkanUtils.h"
#include "Core/VulkanDevice.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace AstralEngine {

/**
 * @enum MemoryType
 * @brief Bellek tipleri için enum
 */
enum class MemoryType {
    DeviceLocal,    ///< GPU hızlı erişim, CPU erişimi yok
    HostVisible,    ///< CPU erişimi var, GPU erişimi yavaş
    HostCoherent,   ///< CPU-GPU otomatik senkronizasyon
    Staging         ///< Geçici transfer için kullanılır
};

/**
 * @enum AllocationStrategy
 * @brief Bellek ayırma stratejileri
 */
enum class AllocationStrategy {
    Linear,         ///< Sıralı ayırma, hızlı ama parçalanmaya eğilimli
    FreeList,       ///< Boş liste, parçalanmayı önler
    Buddy,          ///< Buddy sistem, büyük bloklar için
    Hybrid          ///< Hibrit yaklaşım
};

/**
 * @struct MemoryAllocation
 * @brief Bellek ayırma bilgilerini tutan yapı
 */
struct MemoryAllocation {
    // Varsayılan oluşturucu
    MemoryAllocation() = default;
    
    VkDeviceMemory memory = VK_NULL_HANDLE;     ///< Vulkan bellek nesnesi
    VkDeviceSize offset = 0;                   ///< Bellek içindeki offset
    VkDeviceSize size = 0;                     ///< Ayırılan boyut
    void* mappedData = nullptr;                ///< Eşlenmiş bellek pointer'ı
    MemoryType type = MemoryType::DeviceLocal; ///< Bellek tipi
    uint32_t memoryTypeIndex = 0;              ///< Bellek tipi indeksi
    bool isMapped = false;                     ///< Bellek eşlenmiş mi
    std::string debugName;                     ///< Debug için isim
    
    // RAII için otomatik temizleme
    ~MemoryAllocation();
    
    // Non-copyable
    MemoryAllocation(const MemoryAllocation&) = delete;
    MemoryAllocation& operator=(const MemoryAllocation&) = delete;
    
    // Movable
    MemoryAllocation(MemoryAllocation&& other) noexcept;
    MemoryAllocation& operator=(MemoryAllocation&& other) noexcept;
    
    // Belleği eşle
    void* Map(VkDevice device);
    void Unmap(VkDevice device);
    
    // Bellek ayırma geçerli mi?
    bool IsValid() const { return memory != VK_NULL_HANDLE; }
    
    // Belleği serbest bırak
    void Free(VkDevice device);
};

/**
 * @struct MemoryPool
 * @brief Bellek havuzu yapısı
 */
struct MemoryPool {
    VkDeviceMemory memory = VK_NULL_HANDLE;     ///< Havuzun bellek nesnesi
    VkDeviceSize totalSize = 0;                ///< Toplam havuz boyutu
    VkDeviceSize usedSize = 0;                 ///< Kullanılan boyut
    VkDeviceSize freeSize = 0;                 ///< Boş boyut
    MemoryType type = MemoryType::DeviceLocal; ///< Havuz tipi
    uint32_t memoryTypeIndex = 0;              ///< Bellek tipi indeksi
    void* mappedData = nullptr;                ///< Eşlenmiş bellek pointer'ı
    bool isMapped = false;                     ///< Bellek eşlenmiş mi
    
    // Free list yönetimi için
    struct FreeBlock {
        VkDeviceSize offset = 0;
        VkDeviceSize size = 0;
        bool operator<(const FreeBlock& other) const { return offset < other.offset; }
    };
    
    std::vector<FreeBlock> freeBlocks;         ///< Boş bloklar listesi
    std::vector<bool> usedBlocks;              ///< Kullanılan bloklar (buddy allocator için)
    
    // Havuz istatistikleri
    uint32_t allocationCount = 0;              ///< Toplam ayırma sayısı
    uint32_t freeCount = 0;                    ///< Toplam serbest bırakma sayısı
    
    // Havuz yönetimi
    bool CanAllocate(VkDeviceSize size) const;
    bool Allocate(VkDeviceSize size, VkDeviceSize& outOffset);
    void Free(VkDeviceSize offset, VkDeviceSize size);
    void Defragment();
    
    // Debug bilgileri
    std::string GetDebugInfo() const;
};

/**
 * @class VulkanMemoryManager
 * @brief VMA (Vulkan Memory Allocator) prensiplerini uygulayan bellek yöneticisi
 * 
 * Bu sınıf, Vulkan bellek yönetimini modern ve verimli bir şekilde sağlar.
 * Bellek havuzları, akıllı ayırma stratejileri ve otomatik temizleme
 * mekanizmaları içerir. RAII prensibine uygun olarak tasarlanmıştır.
 */
class VulkanMemoryManager {
public:
    /**
     * @struct Config
     * @brief Bellek yöneticisi yapılandırması
     */
    struct Config {
        VkDeviceSize defaultPoolSize = 256 * 1024 * 1024;  ///< Varsayılan havuz boyutu (256MB)
        VkDeviceSize maxPoolSize = 1024 * 1024 * 1024;      ///< Maksimum havuz boyutu (1GB)
        AllocationStrategy strategy = AllocationStrategy::Hybrid; ///< Ayırma stratejisi
        bool enableDefragmentation = true;                     ///< Otomatik parçalanmayı önleme
        bool enableMemoryTracking = true;                      ///< Bellek takibi
        bool enableLeakDetection = true;                       ///< Bellek sızıntısı tespiti
        bool enableDebugNames = true;                          ///< Debug isimleri
        VkDeviceSize minAllocationSize = 256;                  ///< Minimum ayırma boyutu
        VkDeviceSize alignment = 16;                           ///< Bellek alignment
        
        Config() : defaultPoolSize(256 * 1024 * 1024),
                  maxPoolSize(1024 * 1024 * 1024),
                  strategy(AllocationStrategy::Hybrid),
                  enableDefragmentation(true),
                  enableMemoryTracking(true),
                  enableLeakDetection(true),
                  enableDebugNames(true),
                  minAllocationSize(256),
                  alignment(16) {}
    };

    VulkanMemoryManager();
    ~VulkanMemoryManager();

    // Non-copyable
    VulkanMemoryManager(const VulkanMemoryManager&) = delete;
    VulkanMemoryManager& operator=(const VulkanMemoryManager&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const Config& config = Config());
    void Shutdown();

    // Bellek ayırma
    std::unique_ptr<MemoryAllocation> AllocateBuffer(
        VkDeviceSize size, 
        VkBufferUsageFlags usage, 
        MemoryType type = MemoryType::DeviceLocal,
        const std::string& debugName = ""
    );
    
    std::unique_ptr<MemoryAllocation> AllocateImage(
        VkDeviceSize size, 
        VkImageUsageFlags usage, 
        MemoryType type = MemoryType::DeviceLocal,
        const std::string& debugName = ""
    );
    
    std::unique_ptr<MemoryAllocation> Allocate(
        VkDeviceSize size, 
        MemoryType type = MemoryType::DeviceLocal,
        const std::string& debugName = ""
    );

    // Bellek ayırma (doğrudan VkMemoryRequirements ile)
    std::unique_ptr<MemoryAllocation> AllocateFromRequirements(
        const VkMemoryRequirements& requirements,
        MemoryType type = MemoryType::DeviceLocal,
        const std::string& debugName = ""
    );

    // Belleği serbest bırak
    void Deallocate(std::unique_ptr<MemoryAllocation> allocation);

    // Havuz yönetimi
    std::unique_ptr<MemoryPool> CreatePool(
        MemoryType type, 
        VkDeviceSize size, 
        const std::string& debugName = ""
    );
    
    void DestroyPool(std::unique_ptr<MemoryPool> pool);
    
    // Bellek tipi indeksini bul
    uint32_t FindMemoryTypeIndex(uint32_t typeFilter, MemoryType type);

    // Getter'lar
    VulkanDevice* GetDevice() const { return m_device; }
    const Config& GetConfig() const { return m_config; }
    bool IsInitialized() const { return m_initialized; }

    // İstatistikler ve debug bilgileri
    struct Statistics {
        VkDeviceSize totalAllocated = 0;        ///< Toplam ayrılan bellek
        VkDeviceSize totalUsed = 0;             ///< Toplam kullanılan bellek
        VkDeviceSize totalFree = 0;             ///< Toplam boş bellek
        uint32_t allocationCount = 0;           ///< Toplam ayırma sayısı
        uint32_t deallocationCount = 0;         ///< Toplam serbest bırakma sayısı
        uint32_t poolCount = 0;                 ///< Toplam havuz sayısı
        float fragmentationRatio = 0.0f;        ///< Parçalanma oranı
    };
    
    Statistics GetStatistics() const;
    std::string GetDebugReport() const;
    
    // Bellek takibi ve optimizasyon
    void DefragmentAllPools();
    void CheckForLeaks() const;
    void DumpMemoryMap() const;
    
    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }
    
    // Bellek tipi için özellikleri al
    VkMemoryPropertyFlags GetMemoryPropertyFlags(MemoryType type) const;

private:
    // Yardımcı metodlar
    bool ValidateConfig(const Config& config);
    std::unique_ptr<MemoryPool> CreateMemoryPool(MemoryType type, VkDeviceSize size);
    bool AllocateFromPool(MemoryPool& pool, VkDeviceSize size, VkDeviceSize& outOffset);
    void FreeFromPool(MemoryPool& pool, VkDeviceSize offset, VkDeviceSize size);
    
    // Bellek tipi için uygun havuzu bul veya oluştur
    MemoryPool* FindOrCreatePool(MemoryType type, VkDeviceSize requiredSize);
    
    // Ayırma stratejileri
    bool AllocateLinear(MemoryPool& pool, VkDeviceSize size, VkDeviceSize& outOffset);
    bool AllocateFreeList(MemoryPool& pool, VkDeviceSize size, VkDeviceSize& outOffset);
    bool AllocateBuddy(MemoryPool& pool, VkDeviceSize size, VkDeviceSize& outOffset);
    
    // Bellek birleştirme
    void MergeAdjacentBlocks(MemoryPool& pool);
    
    // Debug ve loglama
    void LogAllocation(const MemoryAllocation& allocation) const;
    void LogDeallocation(const MemoryAllocation& allocation) const;
    void UpdateStatistics();
    
    // Hata yönetimi
    void SetError(const std::string& error);
    void ClearError();
    
    // Thread güvenliği için mutex
    mutable std::mutex m_mutex;
    
    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    Config m_config;
    std::string m_lastError;
    bool m_initialized = false;
    
    // Bellek havuzları
    std::vector<std::unique_ptr<MemoryPool>> m_pools;
    
    // Bellek tipine göre havuz indeksleme
    std::unordered_map<MemoryType, std::vector<size_t>> m_poolsByType;
    
    // İstatistikler
    Statistics m_statistics;
    
    // Bellek takibi için allocation'lar
    std::unordered_map<VkDeviceMemory, std::weak_ptr<MemoryAllocation>> m_allocations;
    
    // Debug için isimlendirme
    std::unordered_map<VkDeviceMemory, std::string> m_allocationNames;
    
    // Atomic sayaçlar
    std::atomic<uint64_t> m_totalAllocations{0};
    std::atomic<uint64_t> m_totalDeallocations{0};
    std::atomic<uint64_t> m_currentAllocations{0};
};

/**
 * @brief MemoryAllocation için RAII deleter
 */
struct MemoryAllocationDeleter {
    VulkanMemoryManager* manager = nullptr;
    
    MemoryAllocationDeleter(VulkanMemoryManager* mgr = nullptr) : manager(mgr) {}
    
    void operator()(MemoryAllocation* allocation) const {
        if (manager && allocation) {
            // Belleği serbest bırak
            allocation->Free(manager->GetDevice()->GetDevice());
            
            // Manager'a bildir
            if (manager->IsInitialized()) {
                std::unique_ptr<MemoryAllocation> ptr(allocation);
                manager->Deallocate(std::move(ptr));
            } else {
                delete allocation;
            }
        }
    }
};

// RAII için smart pointer typedef
using MemoryAllocationPtr = std::unique_ptr<MemoryAllocation, MemoryAllocationDeleter>;

/**
 * @brief Bellek ayırma için yardımcı fonksiyonlar
 */
namespace MemoryUtils {
    /**
     * @brief Buffer için bellek gereksinimlerini al
     */
    VkMemoryRequirements GetBufferMemoryRequirements(VkDevice device, VkBuffer buffer);
    
    /**
     * @brief Image için bellek gereksinimlerini al
     */
    VkMemoryRequirements GetImageMemoryRequirements(VkDevice device, VkImage image);
    
    /**
     * @brief Bellek tipini string'e dönüştür
     */
    std::string MemoryTypeToString(MemoryType type);
    
    /**
     * @brief Ayırma stratejisini string'e dönüştür
     */
    std::string AllocationStrategyToString(AllocationStrategy strategy);
    
    /**
     * @brief Bellek özelliklerini string'e dönüştür
     */
    std::string MemoryPropertyFlagsToString(VkMemoryPropertyFlags flags);
}

} // namespace AstralEngine
