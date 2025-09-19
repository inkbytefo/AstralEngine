#pragma once

#include <cstddef>
#include <memory>

namespace AstralEngine {

/**
 * @brief Motorun merkezi bellek yönetici sınıfı.
 * 
 * Özel bellek ayırıcıları ve havuzları yönetir.
 * Şu anda basit bir wrapper, gelecekte pool allocator,
 * stack allocator ve diğer optimizasyonlar eklenebilir.
 */
class MemoryManager {
public:
    // Singleton pattern
    static MemoryManager& GetInstance();

    // Bellek ayırma ve serbest bırakma
    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
    void Deallocate(void* ptr, size_t alignment = alignof(std::max_align_t));

    // Frame-based temporary allocator
    void* AllocateFrameMemory(size_t size, size_t alignment = alignof(std::max_align_t));
    void ResetFrameMemory();

    // Bellek istatistikleri
    size_t GetTotalAllocated() const { return m_totalAllocated; }
    size_t GetFrameAllocated() const { return m_frameAllocated; }

    // Başlatma ve kapatma
    void Initialize();
    void Shutdown();

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    
    // Non-copyable
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Basit istatistik takibi
    size_t m_totalAllocated = 0;
    size_t m_frameAllocated = 0;
    
    // Frame memory için basit buffer (gelecekte stack allocator ile değiştirilebilir)
    static constexpr size_t FRAME_MEMORY_SIZE = 1024 * 1024; // 1MB
    std::unique_ptr<char[]> m_frameMemoryBuffer;
    size_t m_frameMemoryOffset = 0;
    
    bool m_initialized = false;
};

} // namespace AstralEngine
