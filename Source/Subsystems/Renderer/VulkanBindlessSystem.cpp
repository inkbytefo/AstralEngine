#include "VulkanBindlessSystem.h"
#include "GraphicsDevice.h"
#include <array>

namespace AstralEngine {

bool VulkanBindlessSystem::Initialize(GraphicsDevice* device) {
    m_device = device;
    VkDevice logicalDevice = m_device->GetVulkanDevice()->GetDevice();

    // 1. Create Descriptor Set Layout for Bindless
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    
    // Texture Array
    bindings[0].binding = TEXTURE_BINDING;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = MAX_BINDLESS_RESOURCES;
    bindings[0].stageFlags = VK_SHADER_STAGE_ALL;
    bindings[0].pImmutableSamplers = nullptr;

    // Storage Buffer Array
    bindings[1].binding = STORAGE_BUFFER_BINDING;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = MAX_BINDLESS_RESOURCES;
    bindings[1].stageFlags = VK_SHADER_STAGE_ALL;
    bindings[1].pImmutableSamplers = nullptr;

    // Storage Image Array
    bindings[2].binding = STORAGE_IMAGE_BINDING;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = MAX_BINDLESS_RESOURCES;
    bindings[2].stageFlags = VK_SHADER_STAGE_ALL;
    bindings[2].pImmutableSamplers = nullptr;

    std::array<VkDescriptorBindingFlags, 3> bindingFlags = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo layoutBindingFlags{};
    layoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    layoutBindingFlags.bindingCount = static_cast<uint32_t>(bindingFlags.size());
    layoutBindingFlags.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &layoutBindingFlags;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        Logger::Error("Bindless", "Failed to create bindless descriptor set layout");
        return false;
    }

    // 2. Create Descriptor Pool
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = MAX_BINDLESS_RESOURCES;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = MAX_BINDLESS_RESOURCES;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[2].descriptorCount = MAX_BINDLESS_RESOURCES;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        Logger::Error("Bindless", "Failed to create bindless descriptor pool");
        return false;
    }

    // 3. Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_layout;

    if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        Logger::Error("Bindless", "Failed to allocate bindless descriptor set");
        return false;
    }

    Logger::Info("Bindless", "Bindless System initialized successfully");
    return true;
}

void VulkanBindlessSystem::Shutdown() {
    if (m_device) {
        VkDevice logicalDevice = m_device->GetVulkanDevice()->GetDevice();
        if (m_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(logicalDevice, m_layout, nullptr);
        if (m_descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(logicalDevice, m_descriptorPool, nullptr);
        m_layout = VK_NULL_HANDLE;
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSet = VK_NULL_HANDLE;
    }
}

uint32_t VulkanBindlessSystem::RegisterTexture(VkImageView imageView, VkSampler sampler) {
    uint32_t index = m_textureCount++;
    if (index >= MAX_BINDLESS_RESOURCES) {
        Logger::Error("Bindless", "Exceeded max bindless textures");
        return 0;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = TEXTURE_BINDING;
    write.dstArrayElement = index;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device->GetVulkanDevice()->GetDevice(), 1, &write, 0, nullptr);
    return index;
}

uint32_t VulkanBindlessSystem::RegisterStorageBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    uint32_t index = m_bufferCount++;
    if (index >= MAX_BINDLESS_RESOURCES) {
        Logger::Error("Bindless", "Exceeded max bindless buffers");
        return 0;
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = range;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = STORAGE_BUFFER_BINDING;
    write.dstArrayElement = index;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device->GetVulkanDevice()->GetDevice(), 1, &write, 0, nullptr);
    return index;
}

uint32_t VulkanBindlessSystem::RegisterStorageImage(VkImageView imageView) {
    uint32_t index = m_storageImageCount++;
    if (index >= MAX_BINDLESS_RESOURCES) {
        Logger::Error("Bindless", "Exceeded max bindless storage images");
        return 0;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = STORAGE_IMAGE_BINDING;
    write.dstArrayElement = index;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device->GetVulkanDevice()->GetDevice(), 1, &write, 0, nullptr);
    return index;
}

} // namespace AstralEngine
