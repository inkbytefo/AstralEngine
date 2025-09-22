#include "VulkanTexture.h"
#include "../../../Core/Logger.h"
#include "VulkanBuffer.h" // Staging buffer için
#include "ThirdParty/stb_image.h"

namespace AstralEngine {

VulkanTexture::VulkanTexture()
    : m_graphicsDevice(nullptr), m_device(nullptr) {
}

VulkanTexture::~VulkanTexture() { 
    Shutdown(); 
}

bool VulkanTexture::Initialize(GraphicsDevice* graphicsDevice, const std::string& texturePath) {
    if (m_isInitialized) return true;
    m_graphicsDevice = graphicsDevice;
    m_device = graphicsDevice->GetVulkanDevice();

    try {
        CreateTextureImage(texturePath);
        // Transfer işlemi senkronize olduğu için hemen image view ve sampler oluştur
        CreateTextureImageView();
        CreateTextureSampler();

        m_isInitialized = true;
        m_state = GpuResourceState::Ready;
        Logger::Info("VulkanTexture", "Texture initialization completed successfully: '{}'", texturePath);
        return true;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to initialize texture: ") + e.what());
        Logger::Error("VulkanTexture", "Initialization failed: {}", m_lastError);
        return false;
    }
}

bool VulkanTexture::Initialize(GraphicsDevice* graphicsDevice, const Config& config) {
    if (m_isInitialized) return true;
    m_graphicsDevice = graphicsDevice;
    m_device = graphicsDevice->GetVulkanDevice();

    try {
        // Config bilgilerini sakla
        m_width = config.width;
        m_height = config.height;
        m_format = config.format;

        // Boş bir texture oluştur (post-processing framebuffer'ları için)
        CreateEmptyTexture(config);

        m_isInitialized = true;
        Logger::Info("VulkanTexture", "Empty texture initialization completed: {}x{}, format: {}, name: '{}'",
                    config.width, config.height, static_cast<int>(config.format), config.name);
        return true;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to initialize empty texture: ") + e.what());
        Logger::Error("VulkanTexture", "Empty texture initialization failed: {}", m_lastError);
        return false;
    }
}

bool VulkanTexture::InitializeFromData(GraphicsDevice* graphicsDevice, const void* data, uint32_t width, uint32_t height, VkFormat format) {
    if (m_isInitialized) return true;
    m_graphicsDevice = graphicsDevice;
    m_device = graphicsDevice->GetVulkanDevice();

    try {
        CreateTextureImageFromData(data, width, height, format);
        // Transfer işlemi senkronize olduğu için hemen image view ve sampler oluştur
        CreateTextureImageView();
        CreateTextureSampler();

        m_isInitialized = true;
        m_state = GpuResourceState::Ready;
        Logger::Info("VulkanTexture", "Texture initialization from data completed: {}x{}, format: {}", width, height, static_cast<int>(format));
        return true;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to initialize texture from data: ") + e.what());
        Logger::Error("VulkanTexture", "Initialization from data failed: {}", m_lastError);
        return false;
    }
}

void VulkanTexture::Shutdown() {
    if (!m_isInitialized) return;

    // Doğrudan temizle (GraphicsDevice'in deletion queue'su sadece buffer'lar için)
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
    // Use RAII for stbi image memory management
    auto pixelsDeleter = [](stbi_uc* ptr) {
        if (ptr) stbi_image_free(ptr);
    };
    std::unique_ptr<stbi_uc, decltype(pixelsDeleter)> pixels(
        stbi_load(path.c_str(), &m_texWidth, &m_texHeight, &m_texChannels, STBI_rgb_alpha),
        pixelsDeleter
    );

    VkDeviceSize imageSize = m_texWidth * m_texHeight * 4;

    if (!pixels) {
        SetError("Failed to load texture image: " + path);
        throw std::runtime_error(m_lastError);
    }

    Logger::Debug("VulkanTexture", "Loaded image: {}x{}, {} channels", m_texWidth, m_texHeight, m_texChannels);

    // Boyut ve format bilgilerini sakla
    m_width = static_cast<uint32_t>(m_texWidth);
    m_height = static_cast<uint32_t>(m_texHeight);
    m_format = VK_FORMAT_R8G8B8A8_SRGB;

    // Texture image oluştur
    m_device->CreateImage(m_width, m_height, m_format, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage, m_textureImageMemory);

    // Image layout'ını transfer için hazırla (undefined -> transfer_dst_optimal)
    TransitionImageLayout(m_textureImage, m_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Create temporary staging buffer using shared_ptr to avoid dangling references
    auto staging = std::make_shared<VulkanBuffer>();
    VulkanBuffer::Config stagingConfig{};
    stagingConfig.size = imageSize;
    stagingConfig.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (!staging->Initialize(m_graphicsDevice, stagingConfig)) {
        // Clean up the texture image and memory that were created earlier
        if (m_textureImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device->GetDevice(), m_textureImage, nullptr);
            m_textureImage = VK_NULL_HANDLE;
        }
        if (m_textureImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->GetDevice(), m_textureImageMemory, nullptr);
            m_textureImageMemory = VK_NULL_HANDLE;
        }
        SetError("Failed to create staging buffer");
        throw std::runtime_error(m_lastError);
    }

    // Copy data to staging buffer using centralized interface
    staging->CopyDataFromHost(pixels.get(), imageSize);
    // pixels memory will be automatically freed by unique_ptr when it goes out of scope

    // Set state to uploading before enqueuing transfer
    m_state = GpuResourceState::Uploading;

    // Transfer işlemini VulkanTransferManager ile lambda fonksiyonu kullanarak gerçekleştir
    m_graphicsDevice->GetTransferManager()->QueueTransfer([staging, this](VkCommandBuffer commandBuffer) {
        // Buffer'dan image'e kopyalama işlemini kaydet
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

        vkCmdCopyBufferToImage(commandBuffer, staging->GetBuffer(), m_textureImage,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    });

    // Register cleanup to shutdown the temporary VulkanBuffer via its Shutdown()
    m_graphicsDevice->GetTransferManager()->RegisterCleanupCallback([staging, this]() {
        staging->Shutdown();
        m_state = GpuResourceState::Ready;
    });

    // Image layout'ını shader okuma için hazırla
    TransitionImageLayout(m_textureImage, m_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Logger::Debug("VulkanTexture", "Texture image transfer completed successfully.");
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

    // Boyut ve format bilgilerini sakla
    m_width = width;
    m_height = height;
    m_format = format;

    // Texture image oluştur
    m_device->CreateImage(width, height, format, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage, m_textureImageMemory);

    // Image layout'ını transfer için hazırla (undefined -> transfer_dst_optimal)
    TransitionImageLayout(m_textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Create temporary staging buffer using shared_ptr to avoid dangling references
    auto staging = std::make_shared<VulkanBuffer>();
    VulkanBuffer::Config stagingConfig{};
    stagingConfig.size = imageSize;
    stagingConfig.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingConfig.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (!staging->Initialize(m_graphicsDevice, stagingConfig)) {
        // Clean up the texture image and memory that were created earlier
        if (m_textureImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device->GetDevice(), m_textureImage, nullptr);
            m_textureImage = VK_NULL_HANDLE;
        }
        if (m_textureImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->GetDevice(), m_textureImageMemory, nullptr);
            m_textureImageMemory = VK_NULL_HANDLE;
        }
        SetError("Failed to create staging buffer");
        throw std::runtime_error(m_lastError);
    }

    // Copy data to staging buffer using centralized interface
    staging->CopyDataFromHost(data, imageSize);

    // Set state to uploading before enqueuing transfer
    m_state = GpuResourceState::Uploading;

    // Transfer işlemini VulkanTransferManager ile lambda fonksiyonu kullanarak gerçekleştir
    m_graphicsDevice->GetTransferManager()->QueueTransfer([staging, this, width, height](VkCommandBuffer commandBuffer) {
        // Buffer'dan image'e kopyalama işlemini kaydet
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

        vkCmdCopyBufferToImage(commandBuffer, staging->GetBuffer(), m_textureImage,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    });

    // Register cleanup to shutdown the temporary VulkanBuffer via its Shutdown()
    m_graphicsDevice->GetTransferManager()->RegisterCleanupCallback([staging, this]() {
        staging->Shutdown();
        m_state = GpuResourceState::Ready;
    });

    // Image layout'ını shader okuma için hazırla
    TransitionImageLayout(m_textureImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Logger::Debug("VulkanTexture", "Texture image from data transfer completed successfully.");
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

    // VulkanTransferManager'ın immediate command metodunu kullan
    if (m_graphicsDevice && m_graphicsDevice->GetTransferManager()) {
        VkCommandBuffer commandBuffer = m_graphicsDevice->GetTransferManager()->GetCommandBufferForImmediateUse();

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

        // Command buffer'ı submit et ve bekle
        m_graphicsDevice->GetTransferManager()->SubmitImmediateCommand(commandBuffer);
    } else {
        // Fallback: eski yöntemi kullan
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
}

bool VulkanTexture::IsReady() {
    // Artık transfer işlemleri senkronize olduğu için her zaman true döndür
    // Texture initialize edildiyse kullanıma hazırdır
    return m_isInitialized;
}


void VulkanTexture::SetError(const std::string& error) {
    m_lastError = error;
}

void VulkanTexture::CreateEmptyTexture(const Config& config) {
    // Texture image oluştur (boş data ile)
    m_device->CreateImage(config.width, config.height, config.format, VK_IMAGE_TILING_OPTIMAL,
                          config.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          m_textureImage, m_textureImageMemory);

    // Image layout'ını general olarak ayarla
    // Boş texture'lar için doğrudan SubmitSingleTimeCommands kullanabiliriz çünkü bu senkron bir işlem
    m_device->SubmitSingleTimeCommands([&](VkCommandBuffer commandBuffer) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_textureImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    });

    // Image view ve sampler oluştur
    CreateTextureImageView();
    CreateTextureSampler();

    // Texture durumunu "Ready" olarak ayarla (asenkron upload gerekmiyor)
    m_state = GpuResourceState::Ready;

    Logger::Debug("VulkanTexture", "Empty texture created: {}x{}, format: {}, usage: {}",
                 config.width, config.height, static_cast<int>(config.format), static_cast<uint32_t>(config.usage));
}

} // namespace AstralEngine
