#pragma once

#include "../IRHIResource.h"
#include "../IRHIPipeline.h"
#include "../IRHIDescriptor.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <map>

namespace AstralEngine {

class VulkanDevice;

class VulkanBuffer : public IRHIBuffer {
public:
    VulkanBuffer(VulkanDevice* device, uint64_t size, RHIBufferUsage usage, RHIMemoryProperty memoryProperties);
    ~VulkanBuffer() override;

    uint64_t GetSize() const override { return m_size; }
    void* Map() override;
    void Unmap() override;

    VkBuffer GetBuffer() const { return m_buffer; }
    VmaAllocation GetAllocation() const { return m_allocation; }

private:
    VulkanDevice* m_device;
    uint64_t m_size;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
};

class VulkanTexture : public IRHITexture {
public:
    VulkanTexture(VulkanDevice* device, uint32_t width, uint32_t height, RHIFormat format, RHITextureUsage usage, uint32_t mipLevels = 1, uint32_t arrayLayers = 1);
    // Constructor for swapchain images
    VulkanTexture(VulkanDevice* device, VkImage image, VkImageView view, uint32_t width, uint32_t height, RHIFormat format);
    ~VulkanTexture() override;

    uint32_t GetWidth() const override { return m_width; }
    uint32_t GetHeight() const override { return m_height; }
    RHIFormat GetFormat() const override { return m_format; }
    uint32_t GetMipLevels() const { return m_mipLevels; }
    uint32_t GetArrayLayers() const { return m_arrayLayers; }

    VkImage GetImage() const { return m_image; }
    VkImageView GetImageView() const { return m_imageView; }
    bool IsSwapchainTexture() const { return !m_ownsImage; }

    VkImageLayout GetLayout() const { return m_currentLayout; }
    void SetLayout(VkImageLayout layout) { m_currentLayout = layout; }

    VkImageView GetSubresourceView(uint32_t mipLevel, uint32_t arrayLayer);

private:
    VulkanDevice* m_device;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipLevels;
    uint32_t m_arrayLayers;
    RHIFormat m_format;
    RHITextureUsage m_usage;
    
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    std::map<uint64_t, VkImageView> m_subresourceViews;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    bool m_ownsImage = true;
    VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

class VulkanShader : public IRHIShader {
public:
    VulkanShader(VulkanDevice* device, RHIShaderStage stage, const std::vector<uint8_t>& code);
    ~VulkanShader() override;

    RHIShaderStage GetStage() const override { return m_stage; }
    VkShaderModule GetModule() const { return m_module; }

private:
    VulkanDevice* m_device;
    RHIShaderStage m_stage;
    VkShaderModule m_module = VK_NULL_HANDLE;
};

class VulkanDescriptorSetLayout : public IRHIDescriptorSetLayout {
public:
    VulkanDescriptorSetLayout(VulkanDevice* device, const std::vector<RHIDescriptorSetLayoutBinding>& bindings);
    ~VulkanDescriptorSetLayout() override;

    VkDescriptorSetLayout GetVkLayout() const { return m_layout; }

private:
    VulkanDevice* m_device;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
};

class VulkanDescriptorSet : public IRHIDescriptorSet {
public:
    VulkanDescriptorSet(VulkanDevice* device, VulkanDescriptorSetLayout* layout, VkDescriptorPool pool);
    ~VulkanDescriptorSet() override;

    void UpdateUniformBuffer(uint32_t binding, IRHIBuffer* buffer, uint64_t offset, uint64_t range) override;
    void UpdateCombinedImageSampler(uint32_t binding, IRHITexture* texture, IRHISampler* sampler) override;

    VkDescriptorSet GetVkDescriptorSet() const { return m_set; }

private:
    VulkanDevice* m_device;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    VkDescriptorPool m_pool;
};

class VulkanSampler : public IRHISampler {
public:
    VulkanSampler(VulkanDevice* device, const RHISamplerDescriptor& descriptor);
    ~VulkanSampler() override;

    VkSampler GetVkSampler() const { return m_sampler; }

private:
    VulkanDevice* m_device;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

class VulkanPipeline : public IRHIPipeline {
public:
    VulkanPipeline(VulkanDevice* device, const RHIPipelineStateDescriptor& descriptor);
    ~VulkanPipeline() override;

    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VulkanDevice* m_device;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

} // namespace AstralEngine
