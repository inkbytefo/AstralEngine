#pragma once

#include "../../Core/Engine.h"
#include "../../Core/Logger.h"
#include "IPostProcessingEffect.h"
#include "TonemappingEffect.h"
#include "BloomEffect.h"
#include "RenderSubsystem.h"
#include "VulkanRenderer.h"
#include "GraphicsDevice.h"
#include "Core/VulkanFramebuffer.h"
#include "Buffers/VulkanTexture.h"
#include "Commands/VulkanPipeline.h"
#include "Shaders/VulkanShader.h"
#include "Buffers/VulkanBuffer.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <array>

namespace AstralEngine {

/**
 * @brief Post-processing efektlerini yöneten bileşen
 *
 * Bu bileşen, render boru hattının sonunda uygulanacak olan
 * post-processing efektlerini (Bloom, Tonemapping vb.) yönetir.
 * Ping-pong framebuffer sistemi kullanarak efekt zincirini oluşturur
 * ve RenderSubsystem'den gelen sahne rengini işler.
 */
class PostProcessingSubsystem {
public:
    PostProcessingSubsystem();
    ~PostProcessingSubsystem();

    // Bileşen başlatma ve kapatma metodları
    bool Initialize(RenderSubsystem* owner);
    void Execute(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void Shutdown();

    // Post-processing spesifik metodlar
    /**
     * @brief Input texture'ı ayarlar
     * 
     * @param sceneColorTexture RenderSubsystem'den gelen sahne rengi texture'ı
     */
    void SetInputTexture(VulkanTexture* sceneColorTexture);
    
    /**
     * @brief Yeni bir efekt ekler
     * 
     * @param effect Eklenecek efekt
     */
    void AddEffect(std::unique_ptr<IPostProcessingEffect> effect);
    
    /**
     * @brief İsmi verilen efekti kaldırır
     * 
     * @param effectName Kaldırılacak efektin adı
     */
    void RemoveEffect(const std::string& effectName);
    
    /**
     * @brief Efektin aktif/pasif durumunu ayarlar
     * 
     * @param effectName Efekt adı
     * @param enabled Aktif/pasif durumu
     */
    void EnableEffect(const std::string& effectName, bool enabled);
    
    // Framebuffer erişimi
    /**
     * @brief Ping framebuffer'ını döndürür
     * 
     * @return VulkanFramebuffer* Ping framebuffer pointer'ı
     */
    VulkanFramebuffer* GetPingFramebuffer() const { return m_pingFramebuffer.get(); }
    
    /**
     * @brief Pong framebuffer'ını döndürür
     * 
     * @return VulkanFramebuffer* Pong framebuffer pointer'ı
     */
    VulkanFramebuffer* GetPongFramebuffer() const { return m_pongFramebuffer.get(); }
    
    // Swapchain'e blit işlemi
    void BlitToSwapchain(VkCommandBuffer commandBuffer, VulkanTexture* sourceTexture);
    
    // VulkanRenderer bağlantısı için yeni metod
    void SetVulkanRenderer(VulkanRenderer* renderer);
    
    // Generic efekt erişim metodu
    /**
     * @brief Belirtilen türdeki efekt pointer'ını döndürür
     *
     * @tparam T Efekt tipi (IPostProcessingEffect'ten türemelidir)
     * @return T* Efekt pointer'ı, bulunamazsa nullptr
     */
    template<typename T>
    T* GetEffect() const {
        static_assert(std::is_base_of_v<IPostProcessingEffect, T>, "T must derive from IPostProcessingEffect");
        for (const auto& effect : m_effects) {
            if (auto* castedEffect = dynamic_cast<T*>(effect.get())) {
                return castedEffect;
            }
        }
        return nullptr;
    }

private:
    // Vulkan kaynak yönetimi için yardımcı metodlar
    void CreateFramebuffers(uint32_t width, uint32_t height);
    void CreateRenderPass();
    void CreateFullScreenPipeline();
    void DestroyFramebuffers();
    void DestroyRenderPass();
    void DestroyFullScreenPipeline();
    void DestroyFullScreenQuadBuffers();
    void DestroyDescriptorSets();
    
    // Pipeline oluşturma için yardımcı metodlar
    bool CreateGraphicsPipeline();
    void CreateFullScreenQuadBuffers();
    
    // Shader yükleme yardımcı metodu
    bool LoadShaderCode(const std::string& filePath, std::vector<uint32_t>& spirvCode);
    
    // Descriptor set ve pool yönetimi
    bool CreateDescriptorPoolAndSets();
    bool UpdateDescriptorSet(VulkanTexture* inputTexture);
    
    // Framebuffer yeniden oluşturma metodu
    void RecreateFramebuffers();
    
    // Efekt zincirini işleme
    void ProcessEffectChain(uint32_t frameIndex);
    
    // Üye değişkenler
    RenderSubsystem* m_owner = nullptr;
    VulkanRenderer* m_renderer = nullptr;
    
    // Tam ekran quad için
    uint32_t m_indexCount = 0;
    
    // Ping-pong framebuffer'lar
    std::unique_ptr<VulkanFramebuffer> m_pingFramebuffer;
    std::unique_ptr<VulkanFramebuffer> m_pongFramebuffer;
    
    // Ping-pong texture'lar
    std::unique_ptr<VulkanTexture> m_pingTexture;
    std::unique_ptr<VulkanTexture> m_pongTexture;
    
    // Efekt zinciri
    std::vector<std::unique_ptr<IPostProcessingEffect>> m_effects;
    std::unordered_map<std::string, IPostProcessingEffect*> m_effectNameMap;
    
    // Input texture (RenderSubsystem'den gelen)
    VulkanTexture* m_inputTexture = nullptr;
    
    // Vulkan kaynakları
    VkRenderPass m_postProcessRenderPass = VK_NULL_HANDLE;
    
    // Shader modülleri
    std::unique_ptr<VulkanShader> m_vertexShader;
    std::unique_ptr<VulkanShader> m_fragmentShader;
    
    // Pipeline
    std::unique_ptr<VulkanPipeline> m_fullScreenPipeline;
    VkPipelineLayout m_fullScreenPipelineLayout = VK_NULL_HANDLE;
    
    // Tam ekran quad için buffer'lar
    std::unique_ptr<VulkanBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanBuffer> m_indexBuffer;
    uint32_t m_indexCount = 0;
    
    // Descriptor set ve layout
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    
    // Boyut bilgileri
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    
    // Swapchain yeniden boyutlandırma için
    uint32_t m_currentWidth = 0;
    uint32_t m_currentHeight = 0;
    
    // Başlatma durumu
    bool m_initialized = false;
};

} // namespace AstralEngine