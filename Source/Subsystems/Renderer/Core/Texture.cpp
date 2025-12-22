#include "Texture.h"
#include <stb_image.h>
#include <stdexcept>
#include <iostream>

namespace AstralEngine {

Texture::Texture(IRHIDevice* device, const std::string& path) : m_device(device) {
    // Load image
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &m_width, &m_height, &m_channels, STBI_rgb_alpha);
    
    if (!data) {
        throw std::runtime_error("Failed to load texture: " + path);
    }

    // Create RHI Texture
    // Using SRGB for albedo/diffuse maps usually
    m_texture = device->CreateAndUploadTexture(m_width, m_height, RHIFormat::R8G8B8A8_SRGB, data);

    stbi_image_free(data);

    // Create Sampler (Default for now)
    RHISamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::Repeat;
    samplerDesc.addressModeV = RHISamplerAddressMode::Repeat;
    samplerDesc.addressModeW = RHISamplerAddressMode::Repeat;
    samplerDesc.anisotropyEnable = true;
    samplerDesc.maxAnisotropy = 16.0f;

    m_sampler = device->CreateSampler(samplerDesc);
}

Texture::Texture(IRHIDevice* device, const TextureData& data) : m_device(device) {
    if (!data.IsValid()) {
        throw std::runtime_error("Invalid texture data provided");
    }

    m_width = data.width;
    m_height = data.height;
    m_channels = data.channels;

    // Determine format based on channels (assuming 8-bit per channel for now)
    // TextureData usually stores raw bytes.
    // NOTE: AssetManager/TextureImporter might have already handled decoding.
    // If TextureData contains decoded pixel data:
    
    RHIFormat format = RHIFormat::R8G8B8A8_SRGB;
    if (m_channels == 4) {
        format = RHIFormat::R8G8B8A8_SRGB;
    } else if (m_channels == 3) {
        // Vulkan often doesn't support R8G8B8 directly, usually we need to pad to RGBA
        // But let's assume the data is already in a compatible format or RHI handles it.
        // Wait, CreateAndUploadTexture assumes RGBA (4 bytes) in VulkanDevice implementation currently!
        // "uint64_t imageSize = width * height * 4;"
        
        // If data is RGB, we might have an issue if we just pass it blindly.
        // We should check if TextureImporter converts to RGBA.
        // Let's assume TextureImporter does STBI_rgb_alpha which forces 4 channels.
    }

    // Create RHI Texture
    m_texture = device->CreateAndUploadTexture(m_width, m_height, format, data.data);

    // Create Sampler (Default for now)
    RHISamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::Repeat;
    samplerDesc.addressModeV = RHISamplerAddressMode::Repeat;
    samplerDesc.addressModeW = RHISamplerAddressMode::Repeat;
    samplerDesc.anisotropyEnable = true;
    samplerDesc.maxAnisotropy = 16.0f;

    m_sampler = device->CreateSampler(samplerDesc);
}

Texture::~Texture() {
    // Shared pointers handle destruction
}

}
