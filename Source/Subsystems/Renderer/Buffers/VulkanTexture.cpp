#include "VulkanTexture.h"
#include "../../../Core/Logger.h"
#include "VulkanBuffer.h" // Staging buffer için
#include <stb_image.h>

namespace AstralEngine {

VulkanTexture::VulkanTexture() {}

VulkanTexture::~VulkanTexture() { 
    Shutdown(); 
}

bool VulkanTexture::Initialize(VulkanDevice* device, const std::string& texturePath) {
    if (m_isInitialized) return true;
    m_device = device;

    try {
        CreateTextureImage(texturePath);
        // Not: CreateTextureImageView ve CreateTextureSampler asenkron upload tamamlandığında çağrılacak

        m_isInitialized = true;
        Logger::Info("VulkanTexture", "Texture initialization started successfully: '{}'", texturePath);
        return true;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to initialize texture: ") + e.what());
        Logger::Error("VulkanTexture", "Initialization failed: {}", m_lastError);
        return false;
    }
}

bool VulkanTexture::InitializeFromData(VulkanDevice* device, const void* data, uint32_t width, uint32_t height, VkFormat format) {
    if (m_isInitialized) return true;
    m_device = device;

    try {
        CreateTextureImageFromData(data, width, height, format);
        // Not: CreateTextureImageView ve CreateTextureSampler asenkron upload tamamlandığında çağrılacak

        m_isInitialized = true;
        Logger::Info("VulkanTexture", "Texture initialization from data started: {}x{}, format: {}", width, height, static_cast<int>(format));
        return true;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to initialize texture from data: ") + e.what());
        Logger::Error("VulkanTexture", "Initialization from data failed: {}", m_lastError);
        return false;
    }
}

void VulkanTexture::Shutdown() {
    if (!m_isInitialized) return;

    // Önce staging kaynaklarını temizle
    CleanupStagingResources();

    if (m_textureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device->GetDevice(), m_textureSampler, nullptr);
        m_textureSampler = VK_NULL_HANDLE;
    }
    
    if (m_textureImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device->GetDevice(), m_textureImageView, nullptr);
        m_textureImageView = VK_NULL_HANDLE;
    }
    
    if (m_textureImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device->GetDevice(), m_textureImage, nullptr);
        m_textureImage = VK_NULL_HANDLE;
    }
    
    if (m_textureImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device->GetDevice(), m_textureImageMemory, nullptr);
        m_textureImageMemory = VK_NULL_HANDLE;
    }

    m_isInitialized = false;
    m_state = GpuResourceState::Unloaded;
    Logger::Debug("VulkanTexture", "Texture shutdown complete");
}

void VulkanTexture::CreateTextureImage(const std::string& path) {
    stbi_uc* pixels = stbi_load(path.c_str(), &m_texWidth, &m_texHeight, &m_texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = m_texWidth * m_texHeight * 4;

    if (!pixels) {
        SetError("Failed to load texture image: " + path);
        throw std::runtime_error(m_lastError);
    }

    Logger::Debug("VulkanTexture", "Loaded image: {}x{}, {} channels", m_texWidth, m_texHeight, m_texChannels);

    // Önceki staging kaynaklarını temizle (eğer varsa)
    CleanupStagingResources();

    // Boyut ve format bilgilerini sakla
    m_width = static_cast<uint32_t>(m_texWidth);
    m_height = static_cast<uint32_t>(m_texHeight);
    m_format = VK_FORMAT_R8G8B8A8_SRGB;

    // 1. Geçici bir staging buffer oluştur (CPU'dan erişilebilir)
    m_device->CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           m_stagingBuffer, m_stagingMemory);

    // 2. Staging buffer'ı map et ve veriyi kopyala
    void* mappedData;
    vkMapMemory(m_device->GetDevice(), m_stagingMemory, 0, imageSize, 0, &mappedData);
    memcpy(mappedData, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_device->GetDevice(), m_stagingMemory);

    stbi_image_free(pixels);

    // 3. Texture image oluştur
    m_device->CreateImage(m_width, m_height, m_format, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage, m_textureImageMemory);

    // 4. Image layout'ını transfer için hazırla (undefined -> transfer_dst_optimal)
    TransitionImageLayout(m_textureImage, m_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 5. Asenkron kopyalama komutunu transfer queue'ya gönder
    m_uploadFence = m_device->SubmitToTransferQueue([&](VkCommandBuffer commandBuffer) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_width, m_height, 1};
        
        vkCmdCopyBufferToImage(commandBuffer, m_stagingBuffer, m_textureImage,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    });

    if (m_uploadFence == VK_NULL_HANDLE) {
        SetError("Failed to submit transfer command to queue");
        CleanupStagingResources();
        throw std::runtime_error(m_lastError);
    }

    // 6. Texture durumunu "Uploading" olarak ayarla
    m_state = GpuResourceState::Uploading;

    Logger::Debug("VulkanTexture", "Texture image transfer submitted to transfer queue asynchronously.");
}

void VulkanTexture::CreateTextureImageFromData(const void* data, uint32_t width, uint32_t height, VkFormat format) {
    m_texWidth = width;
    m_texHeight = height;
    m_texChannels = 4; // RGBA varsayılan
    
    // Format'a göre pixel boyutunu hesapla
    VkDeviceSize pixelSize = 0;
    switch (format) {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM:
            pixelSize = 4;
            break;
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8_UNORM:
            pixelSize = 3;
            break;
        case VK_FORMAT_R8_UNORM:
            pixelSize = 1;
            break;
        default:
            SetError("Unsupported texture format for CreateTextureImageFromData");
            throw std::runtime_error(m_lastError);
    }
    
    VkDeviceSize imageSize = width * height * pixelSize;

    if (!data) {
        SetError("Null data pointer provided to CreateTextureImageFromData");
        throw std::runtime_error(m_lastError);
    }

    Logger::Debug("VulkanTexture", "Creating texture from data: {}x{}, format: {}", width, height, static_cast<int>(format));

    // Önceki staging kaynaklarını temizle (eğer varsa)
    CleanupStagingResources();

    // Boyut ve format bilgilerini sakla
    m_width = width;
    m_height = height;
    m_format = format;

    // 1. Geçici bir staging buffer oluştur (CPU'dan erişilebilir)
    m_device->CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           m_stagingBuffer, m_stagingMemory);

    // 2. Staging buffer'ı map et ve veriyi kopyala
    void* mappedData;
    vkMapMemory(m_device->GetDevice(), m_stagingMemory, 0, imageSize, 0, &mappedData);
    memcpy(mappedData, data, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_device->GetDevice(), m_stagingMemory);

    // 3. Texture image oluştur
    m_device->CreateImage(width, height, format, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage, m_textureImageMemory);

    // 4. Image layout'ını transfer için hazırla (undefined -> transfer_dst_optimal)
    TransitionImageLayout(m_textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 5. Asenkron kopyalama komutunu transfer queue'ya gönder
    m_uploadFence = m_device->SubmitToTransferQueue([&](VkCommandBuffer commandBuffer) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        
        vkCmdCopyBufferToImage(commandBuffer, m_stagingBuffer, m_textureImage,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    });

    if (m_uploadFence == VK_NULL_HANDLE) {
        SetError("Failed to submit transfer command to queue");
        CleanupStagingResources();
        throw std::runtime_error(m_lastError);
    }

    // 6. Texture durumunu "Uploading" olarak ayarla
    m_state = GpuResourceState::Uploading;

    Logger::Debug("VulkanTexture", "Texture image from data transfer submitted to transfer queue asynchronously.");
}

void VulkanTexture::CreateTextureImageView() {
    m_textureImageView = m_device->CreateImageView(m_textureImage, m_format, VK_IMAGE_ASPECT_COLOR_BIT);
    Logger::Debug("VulkanTexture", "Texture image view created");
}

void VulkanTexture::CreateTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_device->GetPhysicalDevice(), &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS) {
        SetError("Failed to create texture sampler");
        throw std::runtime_error(m_lastError);
    }

    Logger::Debug("VulkanTexture", "Texture sampler created with max anisotropy: {}", properties.limits.maxSamplerAnisotropy);
}

void VulkanTexture::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    (void)format; // Suppress unused parameter warning
    m_device->SubmitSingleTimeCommands([&](VkCommandBuffer commandBuffer) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            SetError("Unsupported layout transition");
            throw std::invalid_argument(m_lastError);
        }

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    });
}

bool VulkanTexture::IsReady() const {
    if (!m_isInitialized) {
        return false;
    }
    
    // Eğer fence varsa, durumunu kontrol et
    if (m_uploadFence != VK_NULL_HANDLE) {
        VkResult result = vkGetFenceStatus(m_device->GetDevice(), m_uploadFence);
        if (result == VK_SUCCESS) {
            // Fence sinyal verdi, upload tamamlandı
            // Const metod olduğu için state'i değiştiremiyoruz, bu yüzden sadece true döndürüyoruz
            // State güncellemesi için CompleteImageInitialization() çağrılmalı
            return true;
        } else if (result == VK_NOT_READY) {
            // Henüz tamamlanmadı
            return false;
        } else {
            // Hata durumu
            Logger::Error("VulkanTexture", "Fence status check failed: %d", result);
            return false;
        }
    }
    
    // Fence yoksa, state'e göre kontrol et
    return m_state == GpuResourceState::Ready;
}

void VulkanTexture::CleanupStagingResources() {
    if (m_stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device->GetDevice(), m_stagingBuffer, nullptr);
        m_stagingBuffer = VK_NULL_HANDLE;
        Logger::Debug("VulkanTexture", "Staging buffer destroyed");
    }

    if (m_stagingMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device->GetDevice(), m_stagingMemory, nullptr);
        m_stagingMemory = VK_NULL_HANDLE;
        Logger::Debug("VulkanTexture", "Staging memory freed");
    }

    if (m_uploadFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device->GetDevice(), m_uploadFence, nullptr);
        m_uploadFence = VK_NULL_HANDLE;
        Logger::Debug("VulkanTexture", "Upload fence destroyed");
    }
}

void VulkanTexture::CompleteImageInitialization() {
    if (!m_isInitialized || m_state != GpuResourceState::Uploading) {
        Logger::Warning("VulkanTexture", "CompleteImageInitialization called but texture is not in uploading state");
        return;
    }

    // Fence durumunu kontrol et
    if (m_uploadFence != VK_NULL_HANDLE) {
        VkResult result = vkGetFenceStatus(m_device->GetDevice(), m_uploadFence);
        if (result == VK_SUCCESS) {
            // Upload tamamlandı, staging kaynaklarını temizle
            CleanupStagingResources();
            
            // Image layout'ını shader okuma için hazırla
            TransitionImageLayout(m_textureImage, m_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
            // Image view ve sampler oluştur
            CreateTextureImageView();
            CreateTextureSampler();
            
            // Texture durumunu "Ready" olarak ayarla
            m_state = GpuResourceState::Ready;
            
            Logger::Info("VulkanTexture", "Texture initialization completed successfully: {}x{}", m_width, m_height);
        } else if (result == VK_NOT_READY) {
            // Henüz tamamlanmadı, bekle
            Logger::Trace("VulkanTexture", "Texture upload still in progress");
        } else {
            // Hata durumu
            Logger::Error("VulkanTexture", "Fence status check failed: %d", result);
            m_state = GpuResourceState::Failed;
            CleanupStagingResources();
        }
    }
}

void VulkanTexture::SetError(const std::string& error) {
    m_lastError = error;
}

} // namespace AstralEngine
