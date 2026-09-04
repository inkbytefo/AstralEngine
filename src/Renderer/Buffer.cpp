#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/Buffer.hpp"

#include <cstring>
#include <stdexcept>
#include <iostream>

namespace Astral {

uint32_t Buffer::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    if (!m_PhysicalDevice) {
        return 0;
    }
    auto memProps = m_PhysicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("[Astral::Buffer] Uygun bellek tipi bulunamadi!");
}

void Buffer::InitVma(VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocFlags) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = static_cast<VkDeviceSize>(m_Size);
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(m_Usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = memUsage;
    allocCreateInfo.flags = allocFlags;

    if (m_IsPersistentMapped) {
        allocCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo{};
    VkResult result = vmaCreateBuffer(
        m_Allocator,
        &bufferInfo,
        &allocCreateInfo,
        &rawBuffer,
        &m_Allocation,
        &allocInfo
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error("[Astral::Buffer] vmaCreateBuffer basarisiz! VkResult: " + std::to_string(result));
    }

    m_Buffer = rawBuffer;
    if (m_IsPersistentMapped) {
        m_MappedData = allocInfo.pMappedData;
    }
}

void Buffer::InitFallback() {
    if (!m_Device) {
        // Headless mock / test ortami
        return;
    }

    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = m_Size;
    bufferInfo.usage = m_Usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    m_FallbackBuffer = m_Device.createBufferUnique(bufferInfo);
    m_Buffer = m_FallbackBuffer.get();

    auto memReq = m_Device.getBufferMemoryRequirements(m_Buffer);
    uint32_t memTypeIndex = FindMemoryType(memReq.memoryTypeBits, m_Properties);

    vk::MemoryAllocateInfo allocInfo(memReq.size, memTypeIndex);
    m_FallbackMemory = m_Device.allocateMemoryUnique(allocInfo);
    m_Device.bindBufferMemory(m_Buffer, m_FallbackMemory.get(), 0);

    if (m_IsPersistentMapped && (m_Properties & vk::MemoryPropertyFlagBits::eHostVisible)) {
        m_MappedData = m_Device.mapMemory(m_FallbackMemory.get(), 0, m_Size);
    }
}

Buffer::Buffer(
    VmaAllocator allocator,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    VmaMemoryUsage memoryUsage,
    VmaAllocationCreateFlags allocationFlags,
    bool persistentMap
) : m_Allocator(allocator),
    m_Size(size),
    m_Usage(usage),
    m_IsPersistentMapped(persistentMap) {

    if (m_Allocator != VK_NULL_HANDLE) {
        InitVma(memoryUsage, allocationFlags);
    }
}

Buffer::Buffer(
    VmaAllocator allocator,
    vk::Device device,
    vk::PhysicalDevice physicalDevice,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    bool persistentMap
) : m_Allocator(allocator),
    m_Device(device),
    m_PhysicalDevice(physicalDevice),
    m_Size(size),
    m_Usage(usage),
    m_Properties(properties),
    m_IsPersistentMapped(persistentMap) {

    if (m_Allocator != VK_NULL_HANDLE) {
        VmaAllocationCreateFlags allocFlags = 0;
        VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO;
        if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
            allocFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }
        InitVma(memUsage, allocFlags);
    } else {
        InitFallback();
    }
}

Buffer::Buffer(
    vk::Device device,
    vk::PhysicalDevice physicalDevice,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    bool persistentMap
) : Buffer(VK_NULL_HANDLE, device, physicalDevice, size, usage, properties, persistentMap) {
}

Buffer::~Buffer() {
    if (m_Allocator != VK_NULL_HANDLE && m_Buffer) {
        if (m_IsPersistentMapped && m_MappedData) {
            vmaUnmapMemory(m_Allocator, m_Allocation);
            m_MappedData = nullptr;
        }
        vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
        m_Buffer = nullptr;
        m_Allocation = VK_NULL_HANDLE;
    } else {
        if (m_IsPersistentMapped && m_MappedData && m_Device) {
            m_Device.unmapMemory(m_FallbackMemory.get());
            m_MappedData = nullptr;
        }
        m_FallbackBuffer.reset();
        m_FallbackMemory.reset();
        m_Buffer = nullptr;
    }
}

void Buffer::UpdateData(const void* data, size_t size, size_t offset) {
    if (offset + size > m_Size) {
        throw std::out_of_range("[Astral::Buffer] Yazilmak istenen veri boyutu tampon kapasitesini asiyor!");
    }

    if (m_IsPersistentMapped && m_MappedData) {
        std::memcpy(static_cast<char*>(m_MappedData) + offset, data, size);
    } else {
        UpdateDataLegacy(data, size, offset);
    }
}

void Buffer::UpdateDataLegacy(const void* data, size_t size, size_t offset) {
    if (offset + size > m_Size) {
        throw std::out_of_range("[Astral::Buffer] Yazilmak istenen veri boyutu tampon kapasitesini asiyor!");
    }

    if (m_Allocator != VK_NULL_HANDLE && m_Allocation != VK_NULL_HANDLE) {
        void* mappedPtr = nullptr;
        VkResult res = vmaMapMemory(m_Allocator, m_Allocation, &mappedPtr);
        if (res == VK_SUCCESS && mappedPtr) {
            std::memcpy(static_cast<char*>(mappedPtr) + offset, data, size);
            vmaUnmapMemory(m_Allocator, m_Allocation);
        }
    } else if (m_Device && m_FallbackMemory) {
        void* mappedPtr = m_Device.mapMemory(m_FallbackMemory.get(), offset, size);
        std::memcpy(mappedPtr, data, size);
        m_Device.unmapMemory(m_FallbackMemory.get());
    }
}

vk::DeviceMemory Buffer::GetMemory() const {
    if (m_Allocator != VK_NULL_HANDLE && m_Allocation != VK_NULL_HANDLE) {
        VmaAllocationInfo info{};
        vmaGetAllocationInfo(m_Allocator, m_Allocation, &info);
        return vk::DeviceMemory(info.deviceMemory);
    }
    return m_FallbackMemory.get();
}

vk::DescriptorBufferInfo Buffer::GetDescriptorInfo(vk::DeviceSize offset, vk::DeviceSize range) const {
    vk::DescriptorBufferInfo info{};
    info.buffer = m_Buffer;
    info.offset = offset;
    info.range = range;
    return info;
}

} // namespace Astral
