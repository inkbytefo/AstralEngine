#pragma once

#include "Astral/Renderer/VulkanContext.hpp"
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <memory>

namespace Astral {

/**
 * @brief Image-Based Lighting (IBL) Kaynak Yoneticisi (Faz 2)
 * 
 * Karis 2013 sayisal integraliyle CPU uzerinde 512x512 2D BRDF LUT uretir ve
 * fiziksel tabanli prosedurel gokyuzu gradyani ile Irradiance ve Prefiltered
 * Cubemap ortam dokularini hazirlar.
 */
class IBLManager {
public:
    explicit IBLManager(VulkanContext& context);
    ~IBLManager();

    IBLManager(const IBLManager&) = delete;
    IBLManager& operator=(const IBLManager&) = delete;

    [[nodiscard]] vk::ImageView GetBRDFLutView() const noexcept { return m_BrdfLutView.get(); }
    [[nodiscard]] vk::ImageView GetIrradianceView() const noexcept { return m_IrradianceView.get(); }
    [[nodiscard]] vk::ImageView GetPrefilteredView() const noexcept { return m_PrefilteredView.get(); }

    [[nodiscard]] vk::Sampler GetBRDFLutSampler() const noexcept { return m_BrdfLutSampler.get(); }
    [[nodiscard]] vk::Sampler GetCubemapSampler() const noexcept { return m_CubemapSampler.get(); }

    [[nodiscard]] uint32_t GetPrefilteredMipLevels() const noexcept { return m_PrefilteredMipLevels; }

private:
    VulkanContext& m_Context;

    // BRDF 2D LUT (512x512, R16G16_SFLOAT)
    VmaImage m_BrdfLutImage;
    vk::UniqueImageView m_BrdfLutView;
    vk::UniqueSampler m_BrdfLutSampler;

    // Irradiance Cubemap (32x32, 6 yuz, R16G16B16A16_SFLOAT)
    VmaImage m_IrradianceImage;
    vk::UniqueImageView m_IrradianceView;

    // Prefiltered Environment Cubemap (128x128, 5 mips, 6 yuz, R16G16B16A16_SFLOAT)
    VmaImage m_PrefilteredImage;
    vk::UniqueImageView m_PrefilteredView;
    uint32_t m_PrefilteredMipLevels = 5;

    vk::UniqueSampler m_CubemapSampler;

    void CreateSamplers();
    void GenerateBRDFLUT();
    void GenerateProceduralCubemaps();
    void Cleanup();
};

} // namespace Astral
