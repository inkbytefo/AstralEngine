#include "MemoryManager.h"
#include "Logger.h"
#include <cstdlib>
#include <new>
#include <cstdint>

namespace AstralEngine {

MemoryManager& MemoryManager::GetInstance() {
    static MemoryManager instance;
    return instance;
}

void MemoryManager::Initialize() {
    if (m_initialized) {
        Logger::Warning("MemoryManager", "Already initialized");
        return;
    }
    
    // Frame memory buffer'ı ayır
    m_frameMemoryBuffer = std::make_unique<char[]>(FRAME_MEMORY_SIZE);
    m_frameMemoryOffset = 0;
    
    m_initialized = true;
    Logger::Info("MemoryManager", "Memory manager initialized with {}MB frame buffer", 
                FRAME_MEMORY_SIZE / (1024 * 1024));
}

void MemoryManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("MemoryManager", "Shutting down. Total allocated: {} bytes, Frame allocated: {} bytes",
                m_totalAllocated, m_frameAllocated);
    
    m_frameMemoryBuffer.reset();
    m_initialized = false;
}

void* MemoryManager::Allocate(size_t size, size_t alignment) {
    // Şu anda standart malloc kullanıyoruz
    // Gelecekte custom allocator'lar eklenebilir
    
    // MinGW doesn't have std::aligned_alloc, use manual alignment
    if (alignment <= alignof(std::max_align_t)) {
        void* ptr = std::malloc(size);
        if (!ptr) {
            throw std::bad_alloc();
        }
        m_totalAllocated += size;
        return ptr;
    }
    
    // For larger alignments, use manual alignment
    size_t total_size = size + alignment - 1 + sizeof(void*);
    void* raw_ptr = std::malloc(total_size);
    if (!raw_ptr) {
        throw std::bad_alloc();
    }
    
    // Calculate aligned address
    char* aligned_ptr = reinterpret_cast<char*>(raw_ptr) + sizeof(void*);
    aligned_ptr += alignment - (reinterpret_cast<uintptr_t>(aligned_ptr) % alignment);
    
    // Store original pointer before aligned memory
    *(reinterpret_cast<void**>(aligned_ptr) - 1) = raw_ptr;
    
    m_totalAllocated += size;
    return aligned_ptr;
}

void MemoryManager::Deallocate(void* ptr) {
    if (ptr) {
        // Check if this was an aligned allocation
        void* original_ptr = *(reinterpret_cast<void**>(ptr) - 1);
        if (original_ptr && original_ptr != ptr) {
            // This was an aligned allocation, free the original pointer
            std::free(original_ptr);
        } else {
            // Regular allocation
            std::free(ptr);
        }
        // Not: Size tracking için ek metadata gerekiyor
        // Şimdilik basit implementasyon
    }
}

void* MemoryManager::AllocateFrameMemory(size_t size, size_t alignment) {
    if (!m_initialized) {
        Logger::Error("MemoryManager", "Cannot allocate frame memory: not initialized");
        return nullptr;
    }
    
    // Alignment hesapla
    size_t alignedOffset = (m_frameMemoryOffset + alignment - 1) & ~(alignment - 1);
    
    if (alignedOffset + size > FRAME_MEMORY_SIZE) {
        Logger::Error("MemoryManager", "Frame memory buffer overflow! Requested: {}, Available: {}", 
                     size, FRAME_MEMORY_SIZE - alignedOffset);
        return nullptr;
    }
    
    void* ptr = m_frameMemoryBuffer.get() + alignedOffset;
    m_frameMemoryOffset = alignedOffset + size;
    m_frameAllocated += size;
    
    return ptr;
}

void MemoryManager::ResetFrameMemory() {
    m_frameMemoryOffset = 0;
    m_frameAllocated = 0;
}

} // namespace AstralEngine
