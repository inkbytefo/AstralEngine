#pragma once

#include "PostProcessingEffectBase.h"
#include "../Core/Logger.h"
#include "Shaders/VulkanShader.h"
#include "Commands/VulkanPipeline.h"
#include "Buffers/VulkanBuffer.h"
#include "Core/VulkanDevice.h"
#include <glm/glm.hpp>
#include <memory>

namespace AstralEngine {

/**
 * @class TonemappingEffect
 * @brief Tonemapping post-processing efekti implementasyonu
 *
 * Bu sınıf, PostProcessingEffectBase temel sınıfından türetilmiş olup
 * tonemapping işlemlerini gerçekleştirir. Farklı tonemapping algoritmalarını
 * (ACES, Reinhard, Filmic vb.) destekler ve exposure, gamma gibi parametrelerle
 * yapılandırılabilir.
 */
class TonemappingEffect : public PostProcessingEffectBase {
public:
    /**
     * @brief Tonemapping parametreleri için uniform buffer yapısı
     */
    struct TonemappingUBO {
        float exposure;          ///< Exposure değeri
        float gamma;             ///< Gamma correction değeri
        int tonemapper;         ///< Tonemapper tipi (0: None, 1: ACES, 2: Reinhard, 3: Filmic, 4: Custom)
        float contrast;         ///< Kontrast değeri
        float brightness;       ///< Parlaklık değeri
        float saturation;       ///< Doygunluk değeri
        float vignetteIntensity; ///< Vignette yoğunluğu
        float vignetteRadius;    ///< Vignette yarıçapı
        float chromaticAberrationIntensity; ///< Kromatik aberasyon yoğunluğu
        float bloomIntensity;   ///< Bloom yoğunluğu
        float lensDirtIntensity; ///< Lens dirt yoğunluğu
        int useColorGrading;    ///< Color grading kullanımı
        int useDithering;       ///< Dithering kullanımı
        glm::vec2 padding;         ///< Padding (16 byte alignment için)
    };

    /**
     * @brief Push constants yapısı
     */
    struct PushConstants {
        glm::vec2 texelSize;       ///< Texture boyutu bilgisi
        int useBloom;          ///< Bloom kullanımı
        int useVignette;       ///< Vignette kullanımı
        int useChromaticAberration; ///< Kromatik aberasyon kullanımı
    };

    TonemappingEffect();
    virtual ~TonemappingEffect();

    // IPostProcessingEffect arayüz metodları (PostProcessingEffectBase tarafından implemente edilir)
    bool Initialize(VulkanRenderer* renderer) override;
    void Shutdown() override;
    void Update(VulkanTexture* inputTexture, uint32_t frameIndex) override;
    const std::string& GetName() const override;
    bool IsEnabled() const override;
    void SetEnabled(bool enabled) override;

protected:
    // PostProcessingEffectBase sanal metodları
    bool OnInitialize() override;
    void OnShutdown() override;
    bool CreateDescriptorSetLayout() override;
    void UpdateDescriptorSets(VulkanTexture* inputTexture, uint32_t frameIndex) override;

    // Tonemapping parametreleri için getter/setter metodları
    float GetExposure() const { return m_uboData.exposure; }
    void SetExposure(float exposure) { m_uboData.exposure = exposure; }
    
    float GetGamma() const { return m_uboData.gamma; }
    void SetGamma(float gamma) { m_uboData.gamma = gamma; }
    
    int GetTonemapper() const { return m_uboData.tonemapper; }
    void SetTonemapper(int tonemapper) { m_uboData.tonemapper = tonemapper; }
    
    float GetContrast() const { return m_uboData.contrast; }
    void SetContrast(float contrast) { m_uboData.contrast = contrast; }
    
    float GetBrightness() const { return m_uboData.brightness; }
    void SetBrightness(float brightness) { m_uboData.brightness = brightness; }
    
    float GetSaturation() const { return m_uboData.saturation; }
    void SetSaturation(float saturation) { m_uboData.saturation = saturation; }
    
    float GetVignetteIntensity() const { return m_uboData.vignetteIntensity; }
    void SetVignetteIntensity(float intensity) { m_uboData.vignetteIntensity = intensity; }
    
    float GetVignetteRadius() const { return m_uboData.vignetteRadius; }
    void SetVignetteRadius(float radius) { m_uboData.vignetteRadius = radius; }
    
    float GetChromaticAberrationIntensity() const { return m_uboData.chromaticAberrationIntensity; }
    void SetChromaticAberrationIntensity(float intensity) { m_uboData.chromaticAberrationIntensity = intensity; }
    
    float GetBloomIntensity() const { return m_uboData.bloomIntensity; }
    void SetBloomIntensity(float intensity) { m_uboData.bloomIntensity = intensity; }
    
    float GetLensDirtIntensity() const { return m_uboData.lensDirtIntensity; }
    void SetLensDirtIntensity(float intensity) { m_uboData.lensDirtIntensity = intensity; }
    
    bool GetUseColorGrading() const { return m_uboData.useColorGrading != 0; }
    void SetUseColorGrading(bool use) { m_uboData.useColorGrading = use ? 1 : 0; }
    
    bool GetUseDithering() const { return m_uboData.useDithering != 0; }
    void SetUseDithering(bool use) { m_uboData.useDithering = use ? 1 : 0; }

private:
    // Uniform buffer verisi
    TonemappingUBO m_uboData{};
    
    // Push constants verisi
    PushConstants m_pushConstants{};
};

} // namespace AstralEngine