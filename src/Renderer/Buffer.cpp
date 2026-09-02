#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "Astral/Renderer/Buffer.hpp"

#include <cstring>
#include <stdexcept>
#include <iostream>

namespace Astral {

uint32_t Buffer::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    auto memProps = m_PhysicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("[Astral::Buffer] Uygun bellek tipi bulunamadi!");
}

Buffer::Buffer(
    vk::Device device,
    vk::PhysicalDevice physicalDevice,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    bool persistentMap
) : m_Device(device),
    m_PhysicalDevice(physicalDevice),
    m_Size(size),
    m_Usage(usage),
    m_Properties(properties),
    m_IsPersistentMapped(persistentMap) {

    // 1. Buffer olustur
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = m_Size;
    bufferInfo.usage = m_Usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    m_Buffer = m_Device.createBufferUnique(bufferInfo);

    // 2. Bellek gereksinimlerini sorgula ve tahsis et
    auto memReq = m_Device.getBufferMemoryRequirements(m_Buffer.get());
    uint32_t memTypeIndex = FindMemoryType(memReq.memoryTypeBits, m_Properties);

    vk::MemoryAllocateInfo allocInfo(memReq.size, memTypeIndex);
    m_Memory = m_Device.allocateMemoryUnique(allocInfo);
    m_Device.bindBufferMemory(m_Buffer.get(), m_Memory.get(), 0);

    // 3. Kalici esleme (Persistent Mapping)
    if (m_IsPersistentMapped && (m_Properties & vk::MemoryPropertyFlagBits::eHostVisible)) {
        m_MappedData = m_Device.mapMemory(m_Memory.get(), 0, m_Size);
    }
}

Buffer::~Buffer() {
    if (m_IsPersistentMapped && m_MappedData) {
        m_Device.unmapMemory(m_Memory.get());
        m_MappedData = nullptr;
    }
    m_Buffer.reset();
    m_Memory.reset();
}

void Buffer::UpdateData(const void* data, size_t size, size_t offset) {
    if (offset + size > m_Size) {
        throw std::out_of_range("[Astral::Buffer] Yazilmak istenen veri boyutu tampon kapasitesini asiyor!");
    }

    if (m_IsPersistentMapped && m_MappedData) {
        std::memcpy(static_cast<char*>(m_MappedData) + offset, data, size);
    } else {
        // Kalici eslenmemisse gecici olarak esle ve yaz
        UpdateDataLegacy(data, size, offset);
    }
}

void Buffer::UpdateDataLegacy(const void* data, size_t size, size_t offset) {
    if (offset + size > m_Size) {
        throw std::out_of_range("[Astral::Buffer] Yazilmak istenen veri boyutu tampon kapasitesini asiyor!");
    }

    void* mappedPtr = m_Device.mapMemory(m_Memory.get(), offset, size);
    std::memcpy(mappedPtr, data, size);
    m_Device.unmapMemory(m_Memory.get());
}

vk::DescriptorBufferInfo Buffer::GetDescriptorInfo(vk::DeviceSize offset, vk::DeviceSize range) const {
    vk::DescriptorBufferInfo info{};
    info.buffer = m_Buffer.get();
    info.offset = offset;
    info.range = range;
    return info;
}

} // namespace Astral
