#include "VulkanResources.h"
#include "VulkanDevice.h"
#include <stdexcept>
#include <iostream>

namespace AstralEngine {

// Helper to convert RHIDescriptorType to VkDescriptorType
VkDescriptorType GetVkDescriptorType(RHIDescriptorType type) {
    switch (type) {
        case RHIDescriptorType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
        case RHIDescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case RHIDescriptorType::SampledImage: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case RHIDescriptorType::StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case RHIDescriptorType::UniformTexelBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case RHIDescriptorType::StorageTexelBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case RHIDescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case RHIDescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RHIDescriptorType::UniformBufferDynamic: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case RHIDescriptorType::StorageBufferDynamic: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case RHIDescriptorType::InputAttachment: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        default: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

// Helper to convert RHIShaderStage to VkShaderStageFlags
VkShaderStageFlags GetVkShaderStageFlags(RHIShaderStage stage) {
    VkShaderStageFlags flags = 0;
    if (static_cast<int>(stage) & static_cast<int>(RHIShaderStage::Vertex)) flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (static_cast<int>(stage) & static_cast<int>(RHIShaderStage::Fragment)) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (static_cast<int>(stage) & static_cast<int>(RHIShaderStage::Compute)) flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

// --- VulkanBuffer ---

VulkanBuffer::VulkanBuffer(VulkanDevice* device, uint64_t size, RHIBufferUsage usage, RHIMemoryProperty memoryProperties)
    : m_device(device), m_size(size) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = 0;
    
    // Map RHI usage to Vulkan usage
    if (static_cast<int>(usage & RHIBufferUsage::Vertex)) bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (static_cast<int>(usage & RHIBufferUsage::Index)) bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (static_cast<int>(usage & RHIBufferUsage::Uniform)) bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (static_cast<int>(usage & RHIBufferUsage::Storage)) bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (static_cast<int>(usage & RHIBufferUsage::TransferSrc)) bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (static_cast<int>(usage & RHIBufferUsage::TransferDst)) bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
    if (static_cast<int>(memoryProperties & RHIMemoryProperty::DeviceLocal)) {
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        // allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }
    if (static_cast<int>(memoryProperties & RHIMemoryProperty::HostVisible)) {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        if (static_cast<int>(memoryProperties & RHIMemoryProperty::HostCoherent)) {
            // VMA handles this automatically on most platforms, but we can hint
        }
    }

    if (vmaCreateBuffer(device->GetAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }
}

VulkanBuffer::~VulkanBuffer() {
    vmaDestroyBuffer(m_device->GetAllocator(), m_buffer, m_allocation);
}

void* VulkanBuffer::Map() {
    void* mappedData;
    vmaMapMemory(m_device->GetAllocator(), m_allocation, &mappedData);
    return mappedData;
}

void VulkanBuffer::Unmap() {
    vmaUnmapMemory(m_device->GetAllocator(), m_allocation);
}


// --- VulkanTexture ---

VulkanTexture::VulkanTexture(VulkanDevice* device, uint32_t width, uint32_t height, RHIFormat format, RHITextureUsage usage)
    : m_device(device), m_width(width), m_height(height), m_format(format), m_ownsImage(true) {
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    // Map RHI format to Vulkan format
    auto GetVkFormat = [](RHIFormat format) {
        switch (format) {
            case RHIFormat::R8_UNORM: return VK_FORMAT_R8_UNORM;
            case RHIFormat::R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
            case RHIFormat::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
            case RHIFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
            case RHIFormat::B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
            case RHIFormat::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
            case RHIFormat::R16G16_FLOAT: return VK_FORMAT_R16G16_SFLOAT;
            case RHIFormat::R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case RHIFormat::R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
            case RHIFormat::R32G32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
            case RHIFormat::R32G32B32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
            case RHIFormat::R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
            case RHIFormat::D32_FLOAT: return VK_FORMAT_D32_SFLOAT;
            case RHIFormat::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
            case RHIFormat::D32_FLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
            default: return VK_FORMAT_R8G8B8A8_SRGB;
        }
    };
    imageInfo.format = GetVkFormat(format);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = 0;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (static_cast<int>(usage & RHITextureUsage::TransferSrc)) imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (static_cast<int>(usage & RHITextureUsage::TransferDst)) imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (static_cast<int>(usage & RHITextureUsage::Sampled)) imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (static_cast<int>(usage & RHITextureUsage::Storage)) imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (static_cast<int>(usage & RHITextureUsage::ColorAttachment)) imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (static_cast<int>(usage & RHITextureUsage::DepthStencilAttachment)) imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    
    if (vmaCreateImage(device->GetAllocator(), &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB; // TODO: Map properly
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device->GetVkDevice(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
}

VulkanTexture::VulkanTexture(VulkanDevice* device, VkImage image, VkImageView view, uint32_t width, uint32_t height, RHIFormat format)
    : m_device(device), m_image(image), m_imageView(view), m_width(width), m_height(height), m_format(format), m_ownsImage(false) {
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

// --- VulkanDescriptorSetLayout ---

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice* device, const std::vector<RHIDescriptorSetLayoutBinding>& bindings)
    : m_device(device) {
    
    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    for (const auto& binding : bindings) {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = binding.binding;
        vkBinding.descriptorType = GetVkDescriptorType(binding.descriptorType);
        vkBinding.descriptorCount = binding.descriptorCount;
        vkBinding.stageFlags = GetVkShaderStageFlags(binding.stageFlags);
        vkBinding.pImmutableSamplers = nullptr;
        vkBindings.push_back(vkBinding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    layoutInfo.pBindings = vkBindings.data();

    if (vkCreateDescriptorSetLayout(device->GetVkDevice(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(m_device->GetVkDevice(), m_layout, nullptr);
}

// --- VulkanDescriptorSet ---

VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice* device, VulkanDescriptorSetLayout* layout, VkDescriptorPool pool)
    : m_device(device), m_pool(pool) {
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    VkDescriptorSetLayout vkLayout = layout->GetVkLayout();
    allocInfo.pSetLayouts = &vkLayout;

    if (vkAllocateDescriptorSets(device->GetVkDevice(), &allocInfo, &m_set) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set!");
    }
}

VulkanDescriptorSet::~VulkanDescriptorSet() {
    // vkFreeDescriptorSets(m_device->GetVkDevice(), m_pool, 1, &m_set);
}

void VulkanDescriptorSet::UpdateUniformBuffer(uint32_t binding, IRHIBuffer* buffer, uint64_t offset, uint64_t range) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = static_cast<VulkanBuffer*>(buffer)->GetBuffer();
    bufferInfo.offset = offset;
    bufferInfo.range = range;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_set;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device->GetVkDevice(), 1, &descriptorWrite, 0, nullptr);
}

void VulkanDescriptorSet::UpdateCombinedImageSampler(uint32_t binding, IRHITexture* texture, IRHISampler* sampler) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = static_cast<VulkanTexture*>(texture)->GetImageView();
    imageInfo.sampler = static_cast<VulkanSampler*>(sampler)->GetVkSampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_set;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device->GetVkDevice(), 1, &descriptorWrite, 0, nullptr);
}

// --- VulkanSampler ---

VulkanSampler::VulkanSampler(VulkanDevice* device, const RHISamplerDescriptor& descriptor)
    : m_device(device) {
    
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    
    // Min/Mag Filter
    samplerInfo.magFilter = (descriptor.magFilter == RHIFilter::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.minFilter = (descriptor.minFilter == RHIFilter::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

    // Address Mode
    auto GetVkAddressMode = [](RHISamplerAddressMode mode) {
        switch (mode) {
            case RHISamplerAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case RHISamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case RHISamplerAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case RHISamplerAddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    };

    samplerInfo.addressModeU = GetVkAddressMode(descriptor.addressModeU);
    samplerInfo.addressModeV = GetVkAddressMode(descriptor.addressModeV);
    samplerInfo.addressModeW = GetVkAddressMode(descriptor.addressModeW);

    // Anisotropy
    samplerInfo.anisotropyEnable = descriptor.anisotropyEnable ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = descriptor.maxAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device->GetVkDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

VulkanSampler::~VulkanSampler() {
    vkDestroySampler(m_device->GetVkDevice(), m_sampler, nullptr);
}

// --- VulkanPipeline ---

VulkanPipeline::VulkanPipeline(VulkanDevice* device, const RHIPipelineStateDescriptor& descriptor)
    : m_device(device) {
    
    // Vertex Shader
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = static_cast<VulkanShader*>(descriptor.vertexShader)->GetModule();
    vertShaderStageInfo.pName = "main"; // Entry point fixed to main for now, or use specialization

    // Fragment Shader
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = static_cast<VulkanShader*>(descriptor.fragmentShader)->GetModule();
    fragShaderStageInfo.pName = "main";

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    for (const auto& binding : descriptor.vertexBindings) {
        VkVertexInputBindingDescription desc{};
        desc.binding = binding.binding;
        desc.stride = binding.stride;
        desc.inputRate = binding.isInstanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(desc);
    }

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    for (const auto& attr : descriptor.vertexAttributes) {
        VkVertexInputAttributeDescription desc{};
        desc.binding = attr.binding;
        desc.location = attr.location;
        // Map RHI format to Vulkan format
        auto GetVkFormat = [](RHIFormat format) {
            switch (format) {
                case RHIFormat::R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
                case RHIFormat::R32G32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
                case RHIFormat::R32G32B32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
                case RHIFormat::R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
                case RHIFormat::R8_UNORM: return VK_FORMAT_R8_UNORM;
                case RHIFormat::R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
                case RHIFormat::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
                case RHIFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
                default: return VK_FORMAT_UNDEFINED;
            }
        };
        desc.format = GetVkFormat(attr.format);
        if (desc.format == VK_FORMAT_UNDEFINED) {
            throw std::runtime_error("Unsupported vertex attribute format!");
        }
        
        desc.offset = attr.offset;
        attributeDescriptions.push_back(desc);
    }

    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport & Scissor (Dynamic)
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
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // OBJ/OpenGL standard
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = descriptor.depthTestEnabled ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = descriptor.depthWriteEnabled ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; // Standard
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Dynamic States
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
    
    // We assume layout 0 for now or pass descriptors
    // TODO: Handle pipeline layout creation from descriptor set layouts properly
    // For now, we need to pass the descriptor set layout handle if we have one.
    // In this simplified RHI, we might need to store the descriptor set layout in the pipeline descriptor or pass it differently.
    // However, Vulkan requires a PipelineLayout to create a Pipeline.
    // For this prototype, we'll cheat and assume a single descriptor set layout is passed or create a dummy one if needed.
    // Actually, descriptor.layout should point to an IRHIDescriptorSetLayout.
    
    std::vector<VkDescriptorSetLayout> setLayouts;
    // Assuming descriptor.layout is added to RHIPipelineStateDescriptor (it was not in my memory, let's check)
    // Checking RHIPipelineStateDescriptor... it doesn't have layout field.
    // This is a gap in my RHI design. I need to add layout to RHIPipelineStateDescriptor or pass it here.
    
    // For now, let's look at how RenderTest uses it.
    // It calls CreateGraphicsPipeline.
    // We need a PipelineLayout.
    
    // HACK: For now, I will create an empty pipeline layout if none is provided, BUT
    // RenderTest uses descriptor sets. So we need the layout.
    // I should update RHIPipelineStateDescriptor to include `IRHIDescriptorSetLayout* layout`.
    
    // But I can't change RHI definition right now easily without updating other files.
    // Wait, I can try to find if I already added it?
    // Let's assume I missed it.
    
    // FIX: I will use a member variable in VulkanPipeline or pass it via descriptor.
    // Since I cannot change descriptor now without breaking other things potentially, 
    // I'll assume for now we might fail or I need to update RHIPipelineStateDescriptor.
    
    // Let's update RHIPipelineStateDescriptor in IRHIPipeline.h first?
    // Or I can just check if I can get away with empty layout for now (No, shader uses descriptors).
    
    // Wait, previous `VulkanPipeline` implementation must have had something.
    // Let's check `VulkanResources.cpp` old content via memory? No.
    // I'll check `IRHIPipeline.h`.
    
    for (auto* layout : descriptor.descriptorSetLayouts) {
        if (layout) {
            setLayouts.push_back(static_cast<VulkanDescriptorSetLayout*>(layout)->GetVkLayout());
        }
    }

    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device->GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE; // Dynamic Rendering
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    // Dynamic Rendering Info
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    VkFormat colorFormat = device->GetSwapchainImageFormat();
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    
    VkFormat depthFormat = device->GetDepthFormat();
    renderingInfo.depthAttachmentFormat = depthFormat;
    // renderingInfo.stencilAttachmentFormat = depthFormat; // Only if we use stencil
    
    pipelineInfo.pNext = &renderingInfo;

    if (vkCreateGraphicsPipelines(device->GetVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
}

VulkanPipeline::~VulkanPipeline() {
    vkDestroyPipeline(m_device->GetVkDevice(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device->GetVkDevice(), m_layout, nullptr);
}

} // namespace AstralEngine
