#pragma once

#include "Subsystems/Renderer/RHI/IRHIDevice.h"
#include "Subsystems/Asset/AssetData.h"
#include <string>
#include <memory>

namespace AstralEngine {

class Texture {
public:
    Texture(IRHIDevice* device, const std::string& path);
    Texture(IRHIDevice* device, const TextureData& data);
    Texture(IRHIDevice* device, const std::vector<std::string>& facePaths);
    Texture(IRHIDevice* device, std::shared_ptr<IRHITexture> rhiTexture);
    ~Texture();

    static std::shared_ptr<Texture> CreateCubemap(IRHIDevice* device, uint32_t width, uint32_t height, RHIFormat format, uint32_t mipLevels = 1);
    static std::shared_ptr<Texture> CreateFlatTexture(IRHIDevice* device, uint32_t width, uint32_t height, const glm::vec4& color, RHIFormat format = RHIFormat::R8G8B8A8_SRGB);
    static std::shared_ptr<Texture> CreateFlatCubemap(IRHIDevice* device, uint32_t width, uint32_t height, const glm::vec4& color, RHIFormat format = RHIFormat::R8G8B8A8_UNORM);

    IRHITexture* GetRHITexture() const { return m_texture.get(); }
    IRHISampler* GetRHISampler() const { return m_sampler.get(); }

    uint32_t GetWidth() const { return static_cast<uint32_t>(m_width); }
    uint32_t GetHeight() const { return static_cast<uint32_t>(m_height); }
    bool IsCubemap() const { return m_isCubemap; }

private:
    IRHIDevice* m_device;
    std::shared_ptr<IRHITexture> m_texture;
    std::shared_ptr<IRHISampler> m_sampler;
    
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    bool m_isCubemap = false;
};

}
