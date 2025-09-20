#pragma once

#include "PostProcessingEffectBase.h"
#include "../Core/Logger.h"
#include "Core/VulkanFramebuffer.h"
#include "Buffers/VulkanTexture.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace AstralEngine {

/**
 * @class BloomEffect
 * @brief Bloom post-processing efekti implementasyonu
 * 
 * Bu sınıf, PostProcessingEffectBase temel sınıfından türetilmiş olup
 * bloom efektini gerçekleştirir. Bright pass, horizontal blur, vertical blur
 * ve composite adımlarını içerir. Farklı blur kaliteleri ve parametrelerle
 * yapılandırılabilir.
 */
class BloomEffect : public PostProcessingEffectBase {
public:
    /**
     * @brief Bloom parametreleri için uniform buffer yapısı
     */
    struct BloomUBO {
        float threshold;           ///< Bright pass threshold değeri
        float knee;               ///< Soft knee threshold değeri
        float intensity;          ///< Bloom intensity değeri
        float radius;             ///< Blur radius değeri
        int quality;             ///< Blur kalitesi (0: low, 1: medium, 2: high)
        int useDirt;            ///< Lens dirt kullanımı
        float dirtIntensity;     ///< Lens dirt yoğunluğu
        glm::vec2 padding;          ///< Padding (16 byte alignment için)
    };

    /**
     * @brief Push constants yapısı
     */
    struct PushConstants {
        glm::vec2 texelSize;       ///< Texture boyutu bilgisi
        int bloomPass;          ///< Bloom pass tipi (0: bright pass, 1: horizontal blur, 2: vertical blur, 3: composite)
    };

    /**
     * @brief Bloom pass tipleri
     */
    enum class BloomPass {
        BrightPass = 0,        ///< Parlak alanları çıkarma
        HorizontalBlur = 1,    ///< Yatay blur
        VerticalBlur = 2,      ///< Dikey blur
        Composite = 3          ///< Orijinal sahne ile birleştirme
    };

    BloomEffect();
    virtual ~BloomEffect();

    // IPostProcessingEffect arayüz metodları (PostProcessingEffectBase tarafından implemente edilir)
    bool Initialize(VulkanRenderer* renderer) override;
    void Shutdown() override;
    void RecordCommands(VkCommandBuffer commandBuffer, 
                       VulkanTexture* inputTexture,
                       VulkanFramebuffer* outputFramebuffer,
                       uint32_t frameIndex) override;
    const std::string& GetName() const override;
    bool IsEnabled() const override;
    void SetEnabled(bool enabled) override;

    // Bloom parametreleri için getter/setter metodları
    float GetThreshold() const { return m_uboData.threshold; }
    void SetThreshold(float threshold) { m_uboData.threshold = threshold; }
    
    float GetKnee() const { return m_uboData.knee; }
    void SetKnee(float knee) { m_uboData.knee = knee; }
    
    float GetIntensity() const { return m_uboData.intensity; }
    void SetIntensity(float intensity) { m_uboData.intensity = intensity; }
    
    float GetRadius() const { return m_uboData.radius; }
    void SetRadius(float radius) { m_uboData.radius = radius; }
    
    int GetQuality() const { return m_uboData.quality; }
    void SetQuality(int quality) { m_uboData.quality = quality; }
    
    bool GetUseDirt() const { return m_uboData.useDirt != 0; }
    void SetUseDirt(bool use) { m_uboData.useDirt = use ? 1 : 0; }
    
    float GetDirtIntensity() const { return m_uboData.dirtIntensity; }
    void SetDirtIntensity(float intensity) { m_uboData.dirtIntensity = intensity; }

protected:
    // PostProcessingEffectBase sanal metodları
    bool OnInitialize() override;
    void OnShutdown() override;
    void OnRecordCommands(VkCommandBuffer commandBuffer,
                         VulkanTexture* inputTexture,
                         VulkanFramebuffer* outputFramebuffer,
                         uint32_t frameIndex) override;
    bool CreateDescriptorSetLayout() override;
    void UpdateDescriptorSets(VulkanTexture* inputTexture, uint32_t frameIndex) override;

private:
    // Yardımcı metodlar
    bool CreateIntermediateTextures();
    bool CreateIntermediateFramebuffers();
    void RecordPass(VkCommandBuffer commandBuffer, VulkanFramebuffer* targetFramebuffer,
                    VulkanTexture* inputTexture, BloomPass passType, uint32_t frameIndex);
    void UpdateDescriptorSetsForPass(VulkanTexture* inputTexture, uint32_t frameIndex);
    void UpdateCompositeDescriptorSets(VulkanTexture* originalTexture, VulkanTexture* bloomTexture, uint32_t frameIndex);
    void RecordBrightPass(VkCommandBuffer commandBuffer, VulkanTexture* inputTexture, uint32_t frameIndex);
    void RecordHorizontalBlur(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void RecordVerticalBlur(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void RecordComposite(VkCommandBuffer commandBuffer, VulkanTexture* inputTexture,
                        VulkanFramebuffer* outputFramebuffer, uint32_t frameIndex);
    
    // Uniform buffer verisi
    BloomUBO m_uboData{};
    
    // Push constants verisi
    PushConstants m_pushConstants{};
    
    // Bloom efektine özel ara kaynaklar
    std::vector<std::unique_ptr<VulkanTexture>> m_brightPassTextures;
    std::vector<std::unique_ptr<VulkanTexture>> m_blurTextures;
    std::vector<std::unique_ptr<VulkanFramebuffer>> m_brightPassFramebuffers;
    std::vector<std::unique_ptr<VulkanFramebuffer>> m_blurFramebuffers;
    VkRenderPass m_brightPassRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_blurRenderPass = VK_NULL_HANDLE;
};

} // namespace AstralEngine