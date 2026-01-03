#include "Texture.h"
#include <stb_image.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>
#include <glm/glm.hpp>

namespace AstralEngine {

Texture::Texture(IRHIDevice* device, const std::string& path) : m_device(device) {
    // Load image
    stbi_set_flip_vertically_on_load(true);
    
    bool isHDR = stbi_is_hdr(path.c_str());
    void* data = nullptr;
    RHIFormat format = RHIFormat::R8G8B8A8_UNORM;

    if (isHDR) {
        data = stbi_loadf(path.c_str(), &m_width, &m_height, &m_channels, STBI_rgb_alpha);
        format = RHIFormat::R32G32B32A32_FLOAT;
    } else {
        data = stbi_load(path.c_str(), &m_width, &m_height, &m_channels, STBI_rgb_alpha);
        format = RHIFormat::R8G8B8A8_SRGB;
    }
    
    if (!data) {
        throw std::runtime_error("Failed to load texture: " + path);
    }

    // Create RHI Texture
    m_texture = device->CreateAndUploadTexture(m_width, m_height, format, data);

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

    RHIFormat format = RHIFormat::R8G8B8A8_SRGB;
    if (data.isHDR) {
        format = RHIFormat::R32G32B32A32_FLOAT;
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

Texture::Texture(IRHIDevice* device, const std::vector<std::string>& facePaths) : m_device(device) {
    if (facePaths.size() != 6) {
        throw std::runtime_error("Cubemap requires 6 face paths");
    }

    std::vector<void*> faceData(6);
    int width, height, channels;
    bool isHDR = stbi_is_hdr(facePaths[0].c_str());
    RHIFormat format = isHDR ? RHIFormat::R32G32B32A32_FLOAT : RHIFormat::R8G8B8A8_UNORM;

    for (int i = 0; i < 6; ++i) {
        if (isHDR) {
            faceData[i] = stbi_loadf(facePaths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
        } else {
            faceData[i] = stbi_load(facePaths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
        }

        if (!faceData[i]) {
            for (int j = 0; j < i; ++j) stbi_image_free(faceData[j]);
            throw std::runtime_error("Failed to load cubemap face: " + facePaths[i]);
        }
    }

    m_width = width;
    m_height = height;
    m_isCubemap = true;

    std::vector<const void*> constFaceData(faceData.begin(), faceData.end());
    m_texture = device->CreateAndUploadTextureCube(m_width, m_height, format, constFaceData);

    for (int i = 0; i < 6; ++i) {
        stbi_image_free(faceData[i]);
    }

    // Create Sampler for Cubemap
    RHISamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeV = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeW = RHISamplerAddressMode::ClampToEdge;
    
    m_sampler = device->CreateSampler(samplerDesc);
}

std::shared_ptr<Texture> Texture::CreateCubemap(IRHIDevice* device, uint32_t width, uint32_t height, RHIFormat format, uint32_t mipLevels) {
    auto rhiTexture = device->CreateTextureCube(width, height, format, RHITextureUsage::Sampled | RHITextureUsage::ColorAttachment | RHITextureUsage::TransferSrc | RHITextureUsage::TransferDst, mipLevels);
    auto texture = std::make_shared<Texture>(device, rhiTexture);
    texture->m_isCubemap = true;
    
    // Transition to shader read only optimal by default
    auto cmd = device->CreateCommandList();
    cmd->Begin();
    cmd->TransitionImageLayout(rhiTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    cmd->End();
    device->SubmitCommandList(cmd.get());
    device->WaitIdle(); // Ensure transition is complete
    
    // Override sampler for cubemap
    RHISamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeV = RHISamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeW = RHISamplerAddressMode::ClampToEdge;
    texture->m_sampler = device->CreateSampler(samplerDesc);
    
    return texture;
}

std::shared_ptr<Texture> Texture::CreateFlatTexture(IRHIDevice* device, uint32_t width, uint32_t height, const glm::vec4& color, RHIFormat format) {
    uint32_t pixel = 0;
    if (format == RHIFormat::R8G8B8A8_SRGB || format == RHIFormat::R8G8B8A8_UNORM) {
        uint8_t r = static_cast<uint8_t>(color.r * 255.0f);
        uint8_t g = static_cast<uint8_t>(color.g * 255.0f);
        uint8_t b = static_cast<uint8_t>(color.b * 255.0f);
        uint8_t a = static_cast<uint8_t>(color.a * 255.0f);
        pixel = (a << 24) | (b << 16) | (g << 8) | r;
    }

    std::vector<uint32_t> pixels(width * height, pixel);
    auto rhiTexture = device->CreateAndUploadTexture(width, height, format, pixels.data());
    auto texture = std::make_shared<Texture>(device, rhiTexture);

    return texture;
}

std::shared_ptr<Texture> Texture::CreateFlatCubemap(IRHIDevice* device, uint32_t width, uint32_t height, const glm::vec4& color, RHIFormat format) {
    uint32_t pixel = 0;
    if (format == RHIFormat::R8G8B8A8_SRGB || format == RHIFormat::R8G8B8A8_UNORM) {
        uint8_t r = static_cast<uint8_t>(color.r * 255.0f);
        uint8_t g = static_cast<uint8_t>(color.g * 255.0f);
        uint8_t b = static_cast<uint8_t>(color.b * 255.0f);
        uint8_t a = static_cast<uint8_t>(color.a * 255.0f);
        pixel = (a << 24) | (b << 16) | (g << 8) | r;
    }

    std::vector<uint32_t> pixels(width * height, pixel);
    std::vector<const void*> faces(6, pixels.data());
    
    auto rhiTexture = device->CreateAndUploadTextureCube(width, height, format, faces);
    auto texture = std::make_shared<Texture>(device, rhiTexture);
    texture->m_isCubemap = true;

    return texture;
}

Texture::Texture(IRHIDevice* device, std::shared_ptr<IRHITexture> rhiTexture) 
    : m_device(device), m_texture(rhiTexture) {
    
    m_width = rhiTexture->GetWidth();
    m_height = rhiTexture->GetHeight();
    
    // Create Default Sampler
    RHISamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::Repeat;
    samplerDesc.addressModeV = RHISamplerAddressMode::Repeat;
    samplerDesc.addressModeW = RHISamplerAddressMode::Repeat;
    
    m_sampler = device->CreateSampler(samplerDesc);
}

Texture::~Texture() {
    // Shared pointers handle destruction
}

}
