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
    ~Texture();

    IRHITexture* GetRHITexture() const { return m_texture.get(); }
    IRHISampler* GetRHISampler() const { return m_sampler.get(); }

    uint32_t GetWidth() const { return static_cast<uint32_t>(m_width); }
    uint32_t GetHeight() const { return static_cast<uint32_t>(m_height); }

private:
    IRHIDevice* m_device;
    std::shared_ptr<IRHITexture> m_texture;
    std::shared_ptr<IRHISampler> m_sampler;
    
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
};

}
