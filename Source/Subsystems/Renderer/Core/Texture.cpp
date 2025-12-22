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

Texture::~Texture() {
    // Shared pointers handle destruction
}

}
