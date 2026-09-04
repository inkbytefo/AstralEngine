#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <cstdint>
#include <cstddef>

namespace Astral {

class Buffer {
public:
    /// VMA tabanli modern kurucu
    Buffer(
        VmaAllocator allocator,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO,
        VmaAllocationCreateFlags allocationFlags = 0,
        bool persistentMap = true
    );

    /// VMA destekli (allocator null degilse VMA, null ise fallback) kurucu
    Buffer(
        VmaAllocator allocator,
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        bool persistentMap = true
    );

    /// Geriye donuk tam uyumluluk icin eski kurucu (allocator null olarak calisir)
    Buffer(
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        bool persistentMap = true
    );

    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    /// Kalici eslenmis bellek pointer'i uzerinden sifir ek-yukle dogrudan memcpy
    void UpdateData(const void* data, size_t size, size_t offset = 0);

    /// Karsilastirma / Benchmark icin her kare map-unmap yapan yontem
    void UpdateDataLegacy(const void* data, size_t size, size_t offset = 0);

    [[nodiscard]] vk::Buffer GetBuffer() const noexcept { return m_Buffer; }
    [[nodiscard]] vk::DeviceMemory GetMemory() const;
    [[nodiscard]] VmaAllocation GetAllocation() const noexcept { return m_Allocation; }
    [[nodiscard]] vk::DeviceSize GetSize() const noexcept { return m_Size; }
    [[nodiscard]] void* GetMappedData() const noexcept { return m_MappedData; }
    [[nodiscard]] bool IsPersistentMapped() const noexcept { return m_IsPersistentMapped; }
    [[nodiscard]] bool IsVma() const noexcept { return m_Allocator != VK_NULL_HANDLE; }

    vk::DescriptorBufferInfo GetDescriptorInfo(vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE) const;

private:
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    vk::Buffer m_Buffer = nullptr;

    vk::Device m_Device = nullptr;
    vk::PhysicalDevice m_PhysicalDevice = nullptr;
    vk::DeviceSize m_Size = 0;
    vk::BufferUsageFlags m_Usage{};
    vk::MemoryPropertyFlags m_Properties{};
    bool m_IsPersistentMapped = false;
    void* m_MappedData = nullptr;

    // VMA harici fallback durumlar icin
    vk::UniqueBuffer m_FallbackBuffer;
    vk::UniqueDeviceMemory m_FallbackMemory;

    uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    void InitVma(VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocFlags);
    void InitFallback();
};

} // namespace Astral
