#include "VulkanResources.h"
#include "VulkanDevice.h"
#include <stdexcept>
#include <iostream>

namespace AstralEngine {

// --- VulkanBuffer ---

VulkanBuffer::VulkanBuffer(VulkanDevice* device, uint64_t size, RHIBufferUsage usage, RHIMemoryProperty memoryProperties)
    : m_device(device), m_size(size) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = 0;
    
    // Map RHI usage to Vulkan usage
    if (usage == RHIBufferUsage::Vertex) bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage == RHIBufferUsage::Index) bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage == RHIBufferUsage::Uniform) bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage == RHIBufferUsage::Storage) bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage == RHIBufferUsage::TransferSrc) bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage == RHIBufferUsage::TransferDst) bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo{};
    if (memoryProperties == RHIMemoryProperty::DeviceLocal) allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if (memoryProperties == RHIMemoryProperty::HostVisible) {
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    if (memoryProperties == RHIMemoryProperty::HostCoherent) {
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    if (vmaCreateBuffer(device->GetAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }
}

VulkanBuffer::~VulkanBuffer() {
    vmaDestroyBuffer(m_device->GetAllocator(), m_buffer, m_allocation);
}

void* VulkanBuffer::Map() {
    void* data;
    vmaMapMemory(m_device->GetAllocator(), m_allocation, &data);
    return data;
}

void VulkanBuffer::Unmap() {
    vmaUnmapMemory(m_device->GetAllocator(), m_allocation);
}

// --- VulkanTexture ---

VulkanTexture::VulkanTexture(VulkanDevice* device, uint32_t width, uint32_t height, RHIFormat format, RHITextureUsage usage)
    : m_device(device), m_width(width), m_height(height), m_format(format) {
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    
    // Simple format mapping
    VkFormat vkFormat = VK_FORMAT_UNDEFINED;
    if (format == RHIFormat::R8G8B8A8_UNORM) vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
    else if (format == RHIFormat::R8G8B8A8_SRGB) vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
    else if (format == RHIFormat::B8G8R8A8_SRGB) vkFormat = VK_FORMAT_B8G8R8A8_SRGB;
    else if (format == RHIFormat::D32_FLOAT) vkFormat = VK_FORMAT_D32_SFLOAT;
    // Add more mappings as needed
    
    imageInfo.format = vkFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    imageInfo.usage = 0;
    if (usage == RHITextureUsage::Sampled) imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usage == RHITextureUsage::ColorAttachment) imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usage == RHITextureUsage::DepthStencilAttachment) imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage == RHITextureUsage::TransferDst) imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(device->GetAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    // Create ImageView
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == RHIFormat::D32_FLOAT) viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device->GetVkDevice(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
}

VulkanTexture::VulkanTexture(VulkanDevice* device, VkImage image, VkImageView view, uint32_t width, uint32_t height, RHIFormat format)
    : m_device(device), m_width(width), m_height(height), m_format(format), m_image(image), m_imageView(view), m_ownsImage(false) {
}

VulkanTexture::~VulkanTexture() {
    if (m_ownsImage) {
        vkDestroyImageView(m_device->GetVkDevice(), m_imageView, nullptr);
        vmaDestroyImage(m_device->GetAllocator(), m_image, m_allocation);
    }
}

// --- VulkanShader ---

VulkanShader::VulkanShader(VulkanDevice* device, RHIShaderStage stage, const std::vector<uint8_t>& code)
    : m_device(device), m_stage(stage) {
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(device->GetVkDevice(), &createInfo, nullptr, &m_module) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
}

VulkanShader::~VulkanShader() {
    vkDestroyShaderModule(m_device->GetVkDevice(), m_module, nullptr);
}

// --- VulkanPipeline ---

VulkanPipeline::VulkanPipeline(VulkanDevice* device, const RHIPipelineStateDescriptor& descriptor, VkRenderPass renderPass)
    : m_device(device) {
    
    // Shader Stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    if (descriptor.vertexShader) {
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = static_cast<VulkanShader*>(descriptor.vertexShader)->GetModule();
        vertShaderStageInfo.pName = "main";
        shaderStages.push_back(vertShaderStageInfo);
    }
    if (descriptor.fragmentShader) {
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = static_cast<VulkanShader*>(descriptor.fragmentShader)->GetModule();
        fragShaderStageInfo.pName = "main";
        shaderStages.push_back(fragShaderStageInfo);
    }

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    // Placeholder for input bindings/attributes mapping
    // In a real implementation, we map descriptor.vertexBindings and descriptor.vertexAttributes to Vulkan structs
    // For now, assuming empty or manual setup
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport State
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    if (descriptor.cullMode == RHICullMode::None) rasterizer.cullMode = VK_CULL_MODE_NONE;
    else if (descriptor.cullMode == RHICullMode::Front) rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
    
    rasterizer.frontFace = (descriptor.frontFace == RHIFrontFace::CounterClockwise) ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic State
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Push descriptors later
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device->GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Add later
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device->GetVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
}

VulkanPipeline::~VulkanPipeline() {
    vkDestroyPipeline(m_device->GetVkDevice(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device->GetVkDevice(), m_layout, nullptr);
}

} // namespace AstralEngine
