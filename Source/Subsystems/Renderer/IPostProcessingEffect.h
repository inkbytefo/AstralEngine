#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace AstralEngine {

class VulkanRenderer;
class VulkanTexture;
class VulkanFramebuffer;

/**
 * @brief Post-processing efektleri için temel arayüz
 * 
 * Bu arayüz, tüm post-processing efektlerinin (Bloom, Tonemapping vb.)
 * uyması gereken sözleşmeyi tanımlar. Her efekt bu arayüzden türetilmeli
 * ve gerekli metodları implemente etmelidir.
 */
class IPostProcessingEffect {
public:
    virtual ~IPostProcessingEffect() = default;
    
    /**
     * @brief Efekti başlatır
     * 
     * @param renderer Vulkan renderer pointer'ı
     * @return true Başarılı olursa
     * @return false Hata oluşursa
     */
    virtual bool Initialize(VulkanRenderer* renderer) = 0;
    
    /**
     * @brief Efekt kaynaklarını temizler
     */
    virtual void Shutdown() = 0;
    
    /**
     * @brief Efektin render durumunu uygular
     *
     * @param commandBuffer Vulkan command buffer
     * @param input Girdi texture'ı
     * @param output Çıktı framebuffer'ı
     * @param frameIndex Frame indeksi
     */
    virtual void Apply(VkCommandBuffer commandBuffer,
                      VulkanTexture* input,
                      VulkanFramebuffer* output,
                      uint32_t frameIndex) = 0;
    
    /**
     * @brief Efektin adını döndürür
     * 
     * @return const std::string& Efekt adı
     */
    virtual const std::string& GetName() const = 0;
    
    /**
     * @brief Efektin aktif olup olmadığını kontrol eder
     * 
     * @return true Aktifse
     * @return false Pasifse
     */
    virtual bool IsEnabled() const = 0;
    
    /**
     * @brief Efektin aktif/pasif durumunu ayarlar
     * 
     * @param enabled Aktif/pasif durumu
     */
    virtual void SetEnabled(bool enabled) = 0;
};

} // namespace AstralEngine