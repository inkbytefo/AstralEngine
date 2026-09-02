#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <vulkan/vulkan.hpp>
#include <cstdint>
#include <cstddef>

namespace Astral {

class Buffer {
public:
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

    /// Karsilastirma / Benchmark icin her kare map-unmap yapan eski yontem
    void UpdateDataLegacy(const void* data, size_t size, size_t offset = 0);

    vk::Buffer GetBuffer() const { return m_Buffer.get(); }
    vk::DeviceMemory GetMemory() const { return m_Memory.get(); }
    vk::DeviceSize GetSize() const { return m_Size; }
    void* GetMappedData() const { return m_MappedData; }
    bool IsPersistentMapped() const { return m_IsPersistentMapped; }

    vk::DescriptorBufferInfo GetDescriptorInfo(vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE) const;

private:
    vk::Device m_Device;
    vk::PhysicalDevice m_PhysicalDevice;
    vk::DeviceSize m_Size;
    vk::BufferUsageFlags m_Usage;
    vk::MemoryPropertyFlags m_Properties;
    bool m_IsPersistentMapped = false;
    void* m_MappedData = nullptr;

    vk::UniqueBuffer m_Buffer;
    vk::UniqueDeviceMemory m_Memory;

    uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
};

} // namespace Astral
