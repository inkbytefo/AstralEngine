#include "VulkanMemoryManager.h"
#include "../../Core/Logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace AstralEngine {

// MemoryAllocation implementasyonu
MemoryAllocation::~MemoryAllocation() {
    if (IsValid()) {
        // RAII deleter tarafından yönetiliyor, burada doğrudan temizleme yapma
    }
}

MemoryAllocation::MemoryAllocation(MemoryAllocation&& other) noexcept {
    memory = other.memory;
    offset = other.offset;
    size = other.size;
    mappedData = other.mappedData;
    type = other.type;
    memoryTypeIndex = other.memoryTypeIndex;
    isMapped = other.isMapped;
    debugName = std::move(other.debugName);
    
    // Diğer nesneyi geçersiz kıl
    other.memory = VK_NULL_HANDLE;
    other.mappedData = nullptr;
    other.isMapped = false;
}

MemoryAllocation& MemoryAllocation::operator=(MemoryAllocation&& other) noexcept {
    if (this != &other) {
        // Mevcut allocation'ı temizle
        if (IsValid()) {
            // Bu noktada device bilinmiyor, sadece pointer'ları sıfırla
            memory = VK_NULL_HANDLE;
            mappedData = nullptr;
            isMapped = false;
        }
        
        // Diğer nesneden taşı
        memory = other.memory;
        offset = other.offset;
        size = other.size;
        mappedData = other.mappedData;
        type = other.type;
        memoryTypeIndex = other.memoryTypeIndex;
        isMapped = other.isMapped;
        debugName = std::move(other.debugName);
        
        // Diğer nesneyi geçersiz kıl
        other.memory = VK_NULL_HANDLE;
        other.mappedData = nullptr;
        other.isMapped = false;
    }
    
    return *this;
}

void* MemoryAllocation::Map(VkDevice device) {
    if (!IsValid()) {
        return nullptr;
    }
    
    if (isMapped && mappedData) {
        return mappedData;
    }
    
    VkResult result = vkMapMemory(device, memory, offset, size, 0, &mappedData);
    if (result == VK_SUCCESS) {
        isMapped = true;
        return mappedData;
    }
    
    VulkanUtils::LogError("Failed to map memory: " + VulkanUtils::GetVkResultString(result), __FILE__, __LINE__);
    return nullptr;
}

void MemoryAllocation::Unmap(VkDevice device) {
    if (!IsValid() || !isMapped) {
        return;
    }
    
    vkUnmapMemory(device, memory);
    mappedData = nullptr;
    isMapped = false;
}

void MemoryAllocation::Free(VkDevice device) {
    if (!IsValid()) {
        return;
    }
    
    if (isMapped) {
        Unmap(device);
    }
    
    // Belleği serbest bırak (doğrudan vkFreeMemory çağrısı)
    vkFreeMemory(device, memory, nullptr);
    
    // Nesneyi geçersiz kıl
    memory = VK_NULL_HANDLE;
    offset = 0;
    size = 0;
    mappedData = nullptr;
    isMapped = false;
}

// MemoryPool implementasyonu
bool MemoryPool::CanAllocate(VkDeviceSize size) const {
    if (freeBlocks.empty()) {
        return false;
    }
    
    // Uygun boş blok ara
    for (const auto& block : freeBlocks) {
        if (block.size >= size) {
            return true;
        }
    }
    
    return false;
}

bool MemoryPool::Allocate(VkDeviceSize size, VkDeviceSize& outOffset) {
    // Alignment kontrolü
    const VkDeviceSize alignment = 16; // Varsayılan alignment
    VkDeviceSize alignedSize = (size + alignment - 1) & ~(alignment - 1);
    
    // Uygun boş blok ara
    for (auto it = freeBlocks.begin(); it != freeBlocks.end(); ++it) {
        if (it->size >= alignedSize) {
            outOffset = it->offset;
            
            // Bloğu böl veya tamamen kullan
            if (it->size > alignedSize + alignment) {
                // Bloğu böl
                VkDeviceSize remainingSize = it->size - alignedSize;
                VkDeviceSize remainingOffset = it->offset + alignedSize;
                
                // Mevcut bloğu güncelle
                it->size = alignedSize;
                
                // Yeni boş blok ekle
                FreeBlock newBlock;
                newBlock.offset = remainingOffset;
                newBlock.size = remainingSize;
                freeBlocks.push_back(newBlock);
            }
            
            // Bloğu kullanıldı olarak işaretle
            freeBlocks.erase(it);
            
            // İstatistikleri güncelle
            usedSize += alignedSize;
            freeSize -= alignedSize;
            allocationCount++;
            
            return true;
        }
    }
    
    return false;
}

void MemoryPool::Free(VkDeviceSize offset, VkDeviceSize size) {
    // Boş blok ekle
    FreeBlock block;
    block.offset = offset;
    block.size = size;
    freeBlocks.push_back(block);
    
    // Komşu blokları birleştir
    std::sort(freeBlocks.begin(), freeBlocks.end());
    
    for (size_t i = 0; i < freeBlocks.size() - 1; ++i) {
        if (freeBlocks[i].offset + freeBlocks[i].size == freeBlocks[i + 1].offset) {
            // Blokları birleştir
            freeBlocks[i].size += freeBlocks[i + 1].size;
            freeBlocks.erase(freeBlocks.begin() + i + 1);
            i--; // Birleştirmeden sonra tekrar kontrol et
        }
    }
    
    // İstatistikleri güncelle
    usedSize -= size;
    freeSize += size;
    freeCount++;
}

void MemoryPool::Defragment() {
    if (freeBlocks.size() <= 1) {
        return; // Parçalanma yok
    }
    
    // Boş blokları sırala
    std::sort(freeBlocks.begin(), freeBlocks.end());
    
    // Komşu blokları birleştir
    for (size_t i = 0; i < freeBlocks.size() - 1; ++i) {
        if (freeBlocks[i].offset + freeBlocks[i].size == freeBlocks[i + 1].offset) {
            freeBlocks[i].size += freeBlocks[i + 1].size;
            freeBlocks.erase(freeBlocks.begin() + i + 1);
            i--;
        }
    }
}

std::string MemoryPool::GetDebugInfo() const {
    std::ostringstream oss;
    oss << "MemoryPool [Type: " << static_cast<int>(type) << "]\n";
    oss << "  Total Size: " << VulkanUtils::FormatMemorySize(totalSize) << "\n";
    oss << "  Used Size: " << VulkanUtils::FormatMemorySize(usedSize) << "\n";
    oss << "  Free Size: " << VulkanUtils::FormatMemorySize(freeSize) << "\n";
    oss << "  Allocations: " << allocationCount << "\n";
    oss << "  Frees: " << freeCount << "\n";
    oss << "  Free Blocks: " << freeBlocks.size() << "\n";
    oss << "  Fragmentation: " << (freeSize > 0 ? (static_cast<float>(freeBlocks.size()) * 100.0f / (totalSize / 1024)) : 0.0f) << "%";
    
    return oss.str();
}

// VulkanMemoryManager implementasyonu
VulkanMemoryManager::VulkanMemoryManager() = default;

VulkanMemoryManager::~VulkanMemoryManager() {
    Shutdown();
}

bool VulkanMemoryManager::Initialize(VulkanDevice* device, const Config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        SetError("Memory manager already initialized");
        return false;
    }
    
    if (!device) {
        SetError("Invalid device pointer");
        return false;
    }
    
    m_device = device;
    m_config = config;
    
    // Konfigürasyonu doğrula
    if (!ValidateConfig(config)) {
        return false;
    }
    
    // Başlangıç havuzlarını oluştur
    for (int type = static_cast<int>(MemoryType::DeviceLocal); type <= static_cast<int>(MemoryType::Staging); ++type) {
        MemoryType memType = static_cast<MemoryType>(type);
        auto pool = CreateMemoryPool(memType, config.defaultPoolSize);
        if (pool) {
            m_pools.push_back(std::move(pool));
            m_poolsByType[memType].push_back(m_pools.size() - 1);
        }
    }
    
    m_initialized = true;
    VulkanUtils::LogInfo("VulkanMemoryManager initialized successfully", __FILE__, __LINE__);
    
    return true;
}

void VulkanMemoryManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    // Bellek sızıntısı kontrolü
    if (m_config.enableLeakDetection) {
        CheckForLeaks();
    }
    
    // Tüm havuzları temizle
    m_pools.clear();
    m_poolsByType.clear();
    m_allocations.clear();
    m_allocationNames.clear();
    
    m_device = nullptr;
    m_initialized = false;
    
    VulkanUtils::LogInfo("VulkanMemoryManager shutdown completed", __FILE__, __LINE__);
}

std::unique_ptr<MemoryAllocation> VulkanMemoryManager::AllocateBuffer(
    VkDeviceSize size, 
    [[maybe_unused]] VkBufferUsageFlags usage, 
    MemoryType type,
    const std::string& debugName) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        SetError("Memory manager not initialized");
        return nullptr;
    }
    
    // Minimum boyut kontrolü
    if (size < m_config.minAllocationSize) {
        size = m_config.minAllocationSize;
    }
    
    // Alignment kontrolü
    size = (size + m_config.alignment - 1) & ~(m_config.alignment - 1);
    
    // Bellek tipi için uygun havuzu bul
    MemoryPool* pool = FindOrCreatePool(type, size);
    if (!pool) {
        return nullptr;
    }
    
    // Havuzdan ayır
    VkDeviceSize offset = 0;
    if (!AllocateFromPool(*pool, size, offset)) {
        // Yeni havuz oluştur
        auto newPool = CreateMemoryPool(type, std::max(size, m_config.defaultPoolSize));
        if (!newPool) {
            return nullptr;
        }
        
        m_pools.push_back(std::move(newPool));
        m_poolsByType[type].push_back(m_pools.size() - 1);
        pool = m_pools.back().get();
        
        // Yeni havuzdan ayır
        if (!AllocateFromPool(*pool, size, offset)) {
            SetError("Failed to allocate from new pool");
            return nullptr;
        }
    }
    
    // Allocation nesnesi oluştur
    auto allocation = std::make_unique<MemoryAllocation>();
    allocation->memory = pool->memory;
    allocation->offset = offset;
    allocation->size = size;
    allocation->type = type;
    allocation->memoryTypeIndex = pool->memoryTypeIndex;
    allocation->debugName = debugName;
    
    // İstatistikleri güncelle
    m_totalAllocations++;
    m_currentAllocations++;
    UpdateStatistics();
    
    // Debug loglama
    if (m_config.enableMemoryTracking) {
        LogAllocation(*allocation);
    }
    
    return allocation;
}

std::unique_ptr<MemoryAllocation> VulkanMemoryManager::AllocateImage(
    VkDeviceSize size, 
    [[maybe_unused]] VkImageUsageFlags usage, 
    MemoryType type,
    const std::string& debugName) {
    
    // Image allocation, buffer allocation ile aynı mantıkta çalışır
    return Allocate(size, type, debugName);
}

std::unique_ptr<MemoryAllocation> VulkanMemoryManager::Allocate(
    VkDeviceSize size, 
    MemoryType type,
    const std::string& debugName) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        SetError("Memory manager not initialized");
        return nullptr;
    }
    
    // Minimum boyut kontrolü
    if (size < m_config.minAllocationSize) {
        size = m_config.minAllocationSize;
    }
    
    // Alignment kontrolü
    size = (size + m_config.alignment - 1) & ~(m_config.alignment - 1);
    
    // Bellek tipi için uygun havuzu bul
    MemoryPool* pool = FindOrCreatePool(type, size);
    if (!pool) {
        return nullptr;
    }
    
    // Havuzdan ayır
    VkDeviceSize offset = 0;
    if (!AllocateFromPool(*pool, size, offset)) {
        // Yeni havuz oluştur
        auto newPool = CreateMemoryPool(type, std::max(size, m_config.defaultPoolSize));
        if (!newPool) {
            return nullptr;
        }
        
        m_pools.push_back(std::move(newPool));
        m_poolsByType[type].push_back(m_pools.size() - 1);
        pool = m_pools.back().get();
        
        // Yeni havuzdan ayır
        if (!AllocateFromPool(*pool, size, offset)) {
            SetError("Failed to allocate from new pool");
            return nullptr;
        }
    }
    
    // Allocation nesnesi oluştur
    auto allocation = std::make_unique<MemoryAllocation>();
    allocation->memory = pool->memory;
    allocation->offset = offset;
    allocation->size = size;
    allocation->type = type;
    allocation->memoryTypeIndex = pool->memoryTypeIndex;
    allocation->debugName = debugName;
    
    // İstatistikleri güncelle
    m_totalAllocations++;
    m_currentAllocations++;
    UpdateStatistics();
    
    // Debug loglama
    if (m_config.enableMemoryTracking) {
        LogAllocation(*allocation);
    }
    
    return allocation;
}

std::unique_ptr<MemoryAllocation> VulkanMemoryManager::AllocateFromRequirements(
    const VkMemoryRequirements& requirements,
    MemoryType type,
    const std::string& debugName) {
    
    // Gereksinimlere göre ayırma yap
    return Allocate(requirements.size, type, debugName);
}

void VulkanMemoryManager::Deallocate(std::unique_ptr<MemoryAllocation> allocation) {
    if (!allocation || !allocation->IsValid()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Havuzu bul
    for (auto& pool : m_pools) {
        if (pool->memory == allocation->memory) {
            // Havuzdan serbest bırak
            FreeFromPool(*pool, allocation->offset, allocation->size);
            
            // İstatistikleri güncelle
            m_totalDeallocations++;
            m_currentAllocations--;
            UpdateStatistics();
            
            // Debug loglama
            if (m_config.enableMemoryTracking) {
                LogDeallocation(*allocation);
            }
            
            return;
        }
    }
    
    VulkanUtils::LogWarning("Failed to find pool for allocation", __FILE__, __LINE__);
}

std::unique_ptr<MemoryPool> VulkanMemoryManager::CreatePool(
    MemoryType type, 
    VkDeviceSize size, 
    const std::string& debugName) {
    
    if (!m_device) {
        return nullptr;
    }
    
    VkDevice device = m_device->GetDevice();
    [[maybe_unused]] VkPhysicalDevice physicalDevice = m_device->GetPhysicalDevice();
    
    // Bellek tipi indeksini bul
    uint32_t memoryTypeIndex = FindMemoryTypeIndex(0xFFFFFFFF, type);
    if (memoryTypeIndex == 0xFFFFFFFF) {
        SetError("Failed to find suitable memory type");
        return nullptr;
    }
    
    // Bellek ayırma bilgisi
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    // Bellek ayır
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        SetError("Failed to allocate memory: " + VulkanUtils::GetVkResultString(result));
        return nullptr;
    }
    
    // Havuz nesnesi oluştur
    auto pool = std::make_unique<MemoryPool>();
    pool->memory = memory;
    pool->totalSize = size;
    pool->usedSize = 0;
    pool->freeSize = size;
    pool->type = type;
    pool->memoryTypeIndex = memoryTypeIndex;
    
    // İlk boş blok ekle
    MemoryPool::FreeBlock initialBlock;
    initialBlock.offset = 0;
    initialBlock.size = size;
    pool->freeBlocks.push_back(initialBlock);
    
    // Host visible bellek ise eşle
    VkMemoryPropertyFlags properties = GetMemoryPropertyFlags(type);
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        result = vkMapMemory(device, memory, 0, size, 0, &pool->mappedData);
        if (result == VK_SUCCESS) {
            pool->isMapped = true;
        } else {
            VulkanUtils::LogWarning("Failed to map pool memory: " + VulkanUtils::GetVkResultString(result), __FILE__, __LINE__);
        }
    }
    
    VulkanUtils::LogInfo("Created memory pool: " + debugName + " (" + VulkanUtils::FormatMemorySize(size) + ")", __FILE__, __LINE__);
    
    return pool;
}

void VulkanMemoryManager::DestroyPool(std::unique_ptr<MemoryPool> pool) {
    if (!pool || !m_device) {
        return;
    }
    
    VkDevice device = m_device->GetDevice();
    
    // Belleği eşlemi kaldır
    if (pool->isMapped && pool->mappedData) {
        vkUnmapMemory(device, pool->memory);
    }
    
    // Belleği serbest bırak
    vkFreeMemory(device, pool->memory, nullptr);
    
    VulkanUtils::LogInfo("Destroyed memory pool", __FILE__, __LINE__);
}

uint32_t VulkanMemoryManager::FindMemoryTypeIndex(uint32_t typeFilter, MemoryType type) {
    if (!m_device) {
        return 0xFFFFFFFF;
    }
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_device->GetPhysicalDevice(), &memProperties);
    
    VkMemoryPropertyFlags properties = GetMemoryPropertyFlags(type);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    return 0xFFFFFFFF;
}

VulkanMemoryManager::Statistics VulkanMemoryManager::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics;
}

std::string VulkanMemoryManager::GetDebugReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ostringstream oss;
    oss << "=== VulkanMemoryManager Debug Report ===\n";
    oss << "Total Allocations: " << m_totalAllocations.load() << "\n";
    oss << "Total Deallocations: " << m_totalDeallocations.load() << "\n";
    oss << "Current Allocations: " << m_currentAllocations.load() << "\n";
    oss << "Pool Count: " << m_pools.size() << "\n";
    oss << "Total Memory: " << VulkanUtils::FormatMemorySize(m_statistics.totalAllocated) << "\n";
    oss << "Used Memory: " << VulkanUtils::FormatMemorySize(m_statistics.totalUsed) << "\n";
    oss << "Free Memory: " << VulkanUtils::FormatMemorySize(m_statistics.totalFree) << "\n";
    oss << "Fragmentation Ratio: " << std::fixed << std::setprecision(2) << m_statistics.fragmentationRatio << "%\n\n";
    
    // Her havuz için detaylı bilgi
    for (size_t i = 0; i < m_pools.size(); ++i) {
        oss << "Pool " << i << ":\n";
        oss << m_pools[i]->GetDebugInfo() << "\n\n";
    }
    
    return oss.str();
}

void VulkanMemoryManager::DefragmentAllPools() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& pool : m_pools) {
        pool->Defragment();
    }
    
    UpdateStatistics();
    VulkanUtils::LogInfo("Memory defragmentation completed", __FILE__, __LINE__);
}

void VulkanMemoryManager::CheckForLeaks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_currentAllocations.load() > 0) {
        std::ostringstream oss;
        oss << "Memory leak detected! " << m_currentAllocations.load() << " allocations still active";
        VulkanUtils::LogError(oss.str(), __FILE__, __LINE__);
        
        // Aktif allocation'ları listele
        for (const auto& pair : m_allocations) {
            if (auto allocation = pair.second.lock()) {
                VulkanUtils::LogWarning("Leaked allocation: " + allocation->debugName + 
                                      " (" + VulkanUtils::FormatMemorySize(allocation->size) + ")", 
                                      __FILE__, __LINE__);
            }
        }
    }
}

void VulkanMemoryManager::DumpMemoryMap() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string report = GetDebugReport();
    VulkanUtils::LogInfo("Memory Map Dump:\n" + report, __FILE__, __LINE__);
}

VkMemoryPropertyFlags VulkanMemoryManager::GetMemoryPropertyFlags(MemoryType type) const {
    switch (type) {
        case MemoryType::DeviceLocal:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryType::HostVisible:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        case MemoryType::HostCoherent:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case MemoryType::Staging:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        default:
            return 0;
    }
}

// Private metod implementasyonları
bool VulkanMemoryManager::ValidateConfig(const Config& config) {
    if (config.defaultPoolSize == 0) {
        SetError("Default pool size cannot be zero");
        return false;
    }
    
    if (config.maxPoolSize < config.defaultPoolSize) {
        SetError("Max pool size cannot be smaller than default pool size");
        return false;
    }
    
    if (config.minAllocationSize == 0) {
        SetError("Minimum allocation size cannot be zero");
        return false;
    }
    
    if (config.alignment == 0 || (config.alignment & (config.alignment - 1)) != 0) {
        SetError("Alignment must be a power of 2");
        return false;
    }
    
    return true;
}

std::unique_ptr<MemoryPool> VulkanMemoryManager::CreateMemoryPool(MemoryType type, VkDeviceSize size) {
    return CreatePool(type, size, "Auto-created pool");
}

bool VulkanMemoryManager::AllocateFromPool(MemoryPool& pool, VkDeviceSize size, VkDeviceSize& outOffset) {
    switch (m_config.strategy) {
        case AllocationStrategy::Linear:
            return AllocateLinear(pool, size, outOffset);
        case AllocationStrategy::FreeList:
            return AllocateFreeList(pool, size, outOffset);
        case AllocationStrategy::Buddy:
            return AllocateBuddy(pool, size, outOffset);
        case AllocationStrategy::Hybrid:
            // Hybrid: önce free list dene, başarısız olursa linear dene
            if (AllocateFreeList(pool, size, outOffset)) {
                return true;
            }
            return AllocateLinear(pool, size, outOffset);
        default:
            return AllocateFreeList(pool, size, outOffset);
    }
}

void VulkanMemoryManager::FreeFromPool(MemoryPool& pool, VkDeviceSize offset, VkDeviceSize size) {
    pool.Free(offset, size);
}

MemoryPool* VulkanMemoryManager::FindOrCreatePool(MemoryType type, VkDeviceSize requiredSize) {
    // Bu tip için uygun havuz ara
    auto it = m_poolsByType.find(type);
    if (it != m_poolsByType.end()) {
        for (size_t poolIndex : it->second) {
            if (poolIndex < m_pools.size() && m_pools[poolIndex]) {
                auto& pool = m_pools[poolIndex];
                if (pool->CanAllocate(requiredSize)) {
                    return pool.get();
                }
            }
        }
    }
    
    // Uygun havuz bulunamadı, yeni havuz oluştur
    VkDeviceSize poolSize = std::max(requiredSize, m_config.defaultPoolSize);
    if (poolSize > m_config.maxPoolSize) {
        SetError("Required allocation size exceeds maximum pool size");
        return nullptr;
    }
    
    auto newPool = CreateMemoryPool(type, poolSize);
    if (!newPool) {
        return nullptr;
    }
    
    m_pools.push_back(std::move(newPool));
    m_poolsByType[type].push_back(m_pools.size() - 1);
    
    return m_pools.back().get();
}

bool VulkanMemoryManager::AllocateLinear(MemoryPool& pool, VkDeviceSize size, VkDeviceSize& outOffset) {
    // Linear allocation: sadece ilk uygun bloğu kullan
    for (auto it = pool.freeBlocks.begin(); it != pool.freeBlocks.end(); ++it) {
        if (it->size >= size) {
            outOffset = it->offset;
            
            // Bloğu kullan
            VkDeviceSize remainingSize = it->size - size;
            if (remainingSize > 0) {
                // Kalan kısmı yeni boş blok olarak ekle
                MemoryPool::FreeBlock remainingBlock;
                remainingBlock.offset = it->offset + size;
                remainingBlock.size = remainingSize;
                pool.freeBlocks.insert(it + 1, remainingBlock);
            }
            
            pool.freeBlocks.erase(it);
            pool.usedSize += size;
            pool.freeSize -= size;
            pool.allocationCount++;
            
            return true;
        }
    }
    
    return false;
}

bool VulkanMemoryManager::AllocateFreeList(MemoryPool& pool, VkDeviceSize size, VkDeviceSize& outOffset) {
    // Free list allocation: en uygun bloğu bul (best-fit)
    auto bestIt = pool.freeBlocks.end();
    VkDeviceSize bestSize = std::numeric_limits<VkDeviceSize>::max();
    
    for (auto it = pool.freeBlocks.begin(); it != pool.freeBlocks.end(); ++it) {
        if (it->size >= size && it->size < bestSize) {
            bestIt = it;
            bestSize = it->size;
        }
    }
    
    if (bestIt != pool.freeBlocks.end()) {
        outOffset = bestIt->offset;
        
        // Bloğu böl veya tamamen kullan
        if (bestIt->size > size + m_config.alignment) {
            VkDeviceSize remainingSize = bestIt->size - size;
            VkDeviceSize remainingOffset = bestIt->offset + size;
            
            bestIt->size = size;
            
            MemoryPool::FreeBlock newBlock;
            newBlock.offset = remainingOffset;
            newBlock.size = remainingSize;
            pool.freeBlocks.push_back(newBlock);
        }
        
        pool.freeBlocks.erase(bestIt);
        pool.usedSize += size;
        pool.freeSize -= size;
        pool.allocationCount++;
        
        return true;
    }
    
    return false;
}

bool VulkanMemoryManager::AllocateBuddy(MemoryPool& pool, VkDeviceSize size, VkDeviceSize& outOffset) {
    // Buddy system implementation
    VkDeviceSize alignedSize = 1;
    while (alignedSize < size) {
        alignedSize *= 2;
    }
    
    // Buddy blok ara
    for (size_t i = 0; i < pool.freeBlocks.size(); ++i) {
        if (pool.freeBlocks[i].size == alignedSize) {
            outOffset = pool.freeBlocks[i].offset;
            pool.freeBlocks.erase(pool.freeBlocks.begin() + i);
            pool.usedSize += alignedSize;
            pool.freeSize -= alignedSize;
            pool.allocationCount++;
            return true;
        }
    }
    
    // Daha büyük blok böl
    for (size_t i = 0; i < pool.freeBlocks.size(); ++i) {
        if (pool.freeBlocks[i].size > alignedSize) {
            VkDeviceSize blockSize = pool.freeBlocks[i].size;
            VkDeviceSize blockOffset = pool.freeBlocks[i].offset;
            
            // Bloğu ikiye böl
            while (blockSize > alignedSize) {
                blockSize /= 2;
                
                // Sağ bloğu ekle
                MemoryPool::FreeBlock rightBlock;
                rightBlock.offset = blockOffset + blockSize;
                rightBlock.size = blockSize;
                pool.freeBlocks.push_back(rightBlock);
            }
            
            // Sol bloğu kullan
            outOffset = blockOffset;
            pool.freeBlocks.erase(pool.freeBlocks.begin() + i);
            pool.usedSize += alignedSize;
            pool.freeSize -= alignedSize;
            pool.allocationCount++;
            
            return true;
        }
    }
    
    return false;
}

void VulkanMemoryManager::MergeAdjacentBlocks(MemoryPool& pool) {
    std::sort(pool.freeBlocks.begin(), pool.freeBlocks.end());
    
    for (size_t i = 0; i < pool.freeBlocks.size() - 1; ++i) {
        if (pool.freeBlocks[i].offset + pool.freeBlocks[i].size == pool.freeBlocks[i + 1].offset) {
            pool.freeBlocks[i].size += pool.freeBlocks[i + 1].size;
            pool.freeBlocks.erase(pool.freeBlocks.begin() + i + 1);
            i--;
        }
    }
}

void VulkanMemoryManager::LogAllocation(const MemoryAllocation& allocation) const {
    std::ostringstream oss;
    oss << "Allocated: " << VulkanUtils::FormatMemorySize(allocation.size) 
        << " [Type: " << static_cast<int>(allocation.type) 
        << ", Offset: " << allocation.offset 
        << ", Name: " << (allocation.debugName.empty() ? "unnamed" : allocation.debugName) << "]";
    VulkanUtils::LogDebug(oss.str(), __FILE__, __LINE__);
}

void VulkanMemoryManager::LogDeallocation(const MemoryAllocation& allocation) const {
    std::ostringstream oss;
    oss << "Deallocated: " << VulkanUtils::FormatMemorySize(allocation.size) 
        << " [Type: " << static_cast<int>(allocation.type) 
        << ", Offset: " << allocation.offset 
        << ", Name: " << (allocation.debugName.empty() ? "unnamed" : allocation.debugName) << "]";
    VulkanUtils::LogDebug(oss.str(), __FILE__, __LINE__);
}

void VulkanMemoryManager::UpdateStatistics() {
    m_statistics.totalAllocated = 0;
    m_statistics.totalUsed = 0;
    m_statistics.totalFree = 0;
    m_statistics.allocationCount = 0;
    m_statistics.deallocationCount = 0;
    m_statistics.poolCount = static_cast<uint32_t>(m_pools.size());
    
    uint32_t totalFreeBlocks = 0;
    
    for (const auto& pool : m_pools) {
        m_statistics.totalAllocated += pool->totalSize;
        m_statistics.totalUsed += pool->usedSize;
        m_statistics.totalFree += pool->freeSize;
        m_statistics.allocationCount += pool->allocationCount;
        m_statistics.deallocationCount += pool->freeCount;
        totalFreeBlocks += static_cast<uint32_t>(pool->freeBlocks.size());
    }
    
    // Parçalanma oranını hesapla
    if (m_statistics.totalFree > 0) {
        m_statistics.fragmentationRatio = (static_cast<float>(totalFreeBlocks) * 100.0f) / 
                                       (static_cast<float>(m_statistics.totalFree) / (1024.0f * 1024.0f));
    } else {
        m_statistics.fragmentationRatio = 0.0f;
    }
}

void VulkanMemoryManager::SetError(const std::string& error) {
    m_lastError = error;
    VulkanUtils::LogError(error, __FILE__, __LINE__);
}

void VulkanMemoryManager::ClearError() {
    m_lastError.clear();
}

// MemoryUtils namespace implementasyonu
namespace MemoryUtils {

VkMemoryRequirements GetBufferMemoryRequirements(VkDevice device, VkBuffer buffer) {
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    return requirements;
}

VkMemoryRequirements GetImageMemoryRequirements(VkDevice device, VkImage image) {
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, image, &requirements);
    return requirements;
}

std::string MemoryTypeToString(MemoryType type) {
    switch (type) {
        case MemoryType::DeviceLocal: return "DeviceLocal";
        case MemoryType::HostVisible: return "HostVisible";
        case MemoryType::HostCoherent: return "HostCoherent";
        case MemoryType::Staging: return "Staging";
        default: return "Unknown";
    }
}

std::string AllocationStrategyToString(AllocationStrategy strategy) {
    switch (strategy) {
        case AllocationStrategy::Linear: return "Linear";
        case AllocationStrategy::FreeList: return "FreeList";
        case AllocationStrategy::Buddy: return "Buddy";
        case AllocationStrategy::Hybrid: return "Hybrid";
        default: return "Unknown";
    }
}

std::string MemoryPropertyFlagsToString(VkMemoryPropertyFlags flags) {
    std::vector<std::pair<VkMemoryPropertyFlagBits, const char*>> propertyNames = {
        {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "DEVICE_LOCAL"},
        {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "HOST_VISIBLE"},
        {VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "HOST_COHERENT"},
        {VK_MEMORY_PROPERTY_HOST_CACHED_BIT, "HOST_CACHED"},
        {VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, "LAZILY_ALLOCATED"},
        {VK_MEMORY_PROPERTY_PROTECTED_BIT, "PROTECTED"},
        {VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD, "DEVICE_COHERENT_AMD"},
        {VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD, "DEVICE_UNCACHED_AMD"},
        {VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV, "RDMA_CAPABLE_NV"}
    };
    
    if (flags == 0) {
        return "NONE";
    }
    
    std::vector<std::string> activeFlags;
    
    for (const auto& propPair : propertyNames) {
        if (flags & propPair.first) {
            activeFlags.push_back(propPair.second);
        }
    }
    
    if (activeFlags.empty()) {
        std::ostringstream oss;
        oss << "UNKNOWN_" << std::hex << static_cast<uint32_t>(flags);
        return oss.str();
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < activeFlags.size(); ++i) {
        if (i > 0) {
            oss << " | ";
        }
        oss << activeFlags[i];
    }
    
    return oss.str();
}

} // namespace MemoryUtils

} // namespace AstralEngine
