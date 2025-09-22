#pragma once

#include "IPostProcessingEffect.h"
#include "../../Core/Logger.h"
#include "Shaders/VulkanShader.h"
#include "Commands/VulkanPipeline.h"
#include "Buffers/VulkanBuffer.h"
#include "Core/VulkanDevice.h"
#include "../Asset/AssetData.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>

// Forward declarations
class VulkanRenderer;
class VulkanTexture;
class VulkanFramebuffer;

// Vertex structure for full-screen quad
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

namespace AstralEngine {

/**
 * @class PostProcessingEffectBase
 * @brief Post-processing efektleri için temel sınıf
 * 
 * Bu sınıf, IPostProcessingEffect arayüzünden türetilmiş olup
 * tüm post-processing efektlerinde ortak olan işlevselliği sağlar.
 * Vulkan kaynak yönetimi, shader yükleme, pipeline oluşturma, descriptor set
 * layout oluşturma ve tam ekran quad çizme gibi ortak işlemleri içerir.
 */
class PostProcessingEffectBase : public IPostProcessingEffect {
public:
    /**
     * @brief Base konfigürasyon yapısı
     * 
     * Post-processing efektlerinin temel yapılandırma parametrelerini içerir.
     */
    struct BaseConfig {
        std::string name;                     ///< Efekt adı
        std::string vertexShaderPath;         ///< Vertex shader yolu
        std::string fragmentShaderPath;       ///< Fragment shader yolu
        uint32_t frameCount;                  ///< Frame sayısı (swapchain image sayısı)
        uint32_t width;                       ///< Genişlik
        uint32_t height;                      ///< Yükseklik
        bool useMinimalVertexInput;           ///< Minimal vertex input kullanımı
    };

    /**
     * @brief Post-processing render state yapısı
     *
     * Efektin render durumu hakkında bilgi içerir. Renderer bu bilgiyi
     * kullanarak efektin nasıl render edileceğini belirler.
     */
    struct PostProcessingRenderState {
        VulkanShader* vertexShader = nullptr;
        VulkanShader* fragmentShader = nullptr;
        // Add other states like blend modes, depth testing, etc. as needed
    };

    PostProcessingEffectBase();
    virtual ~PostProcessingEffectBase();

    // --- MODIFY IPostProcessingEffect interface implementation ---
    bool Initialize(VulkanRenderer* renderer) override;
    void Shutdown() override;
    
    // --- ADD methods for the renderer to query the effect's state ---
    virtual VulkanPipeline* GetPipeline() const { return m_pipeline.get(); }
    virtual VkDescriptorSet GetCurrentDescriptorSet(uint32_t frameIndex) const {
        return (frameIndex < m_descriptorSets.size()) ? m_descriptorSets[frameIndex] : VK_NULL_HANDLE;
    }
    virtual void UpdateDescriptorSets(VulkanTexture* inputTexture, uint32_t frameIndex) = 0;
    
    const std::string& GetName() const override;
    bool IsEnabled() const override;
    void SetEnabled(bool enabled) override;

protected:
    /**
     * @brief Efekt özel başlatma metodunu implemente et
     * 
     * Türetilmiş sınıflar bu metodu override ederek efekt özel
     * başlatma işlemlerini yapmalıdır.
     * 
     * @return true Başarılı olursa
     * @return false Hata oluşursa
     */
    virtual bool OnInitialize() = 0;

    /**
     * @brief Efekt özel kapatma metodunu implemente et
     * 
     * Türetilmiş sınıflar bu metodu override ederek efekt özel
     * kapatma işlemlerini yapmalıdır.
     */
    virtual void OnShutdown() = 0;


    // --- MODIFY resource creation methods to be more abstract ---
    bool CreateShaders(const std::string& vertexPath, const std::string& fragmentPath);
    // Pipeline creation will be handled by the renderer based on the effect's state.
    // bool CreatePipeline();
    bool CreateUniformBuffers(size_t uboSize);
    bool CreateFullScreenQuadBuffer();
    
    // --- REMOVE direct Vulkan resource management from protected interface ---
    // virtual bool CreateDescriptorSetLayout() = 0;
    // virtual void UpdateDescriptorSets(...) = 0;
    // void DrawFullScreenQuad(...);
    
    // Hata yönetimi
    void SetError(const std::string& error);
    const std::string& GetLastError() const;

    // Getter metodları
    VulkanRenderer* GetRenderer() const { return m_renderer; }
    VulkanDevice* GetDevice() const { return m_device; }
    VulkanShader* GetVertexShader() const { return m_vertexShader.get(); }
    VulkanShader* GetFragmentShader() const { return m_fragmentShader.get(); }
    VulkanPipeline* GetPipeline() const { return m_pipeline.get(); }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }
    VkDescriptorPool GetDescriptorPool() const { return m_descriptorPool; }
    const std::vector<VkDescriptorSet>& GetDescriptorSets() const { return m_descriptorSets; }
    const std::vector<std::unique_ptr<VulkanBuffer>>& GetUniformBuffers() const { return m_uniformBuffers; }
    VulkanBuffer* GetVertexBuffer() const { return m_vertexBuffer.get(); }
    uint32_t GetVertexCount() const { return m_vertexCount; }
    uint32_t GetFrameCount() const { return m_frameCount; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    bool IsInitialized() const { return m_isInitialized; }

    // Setter metodları
    void SetConfig(const BaseConfig& config) { m_config = config; }
    void SetName(const std::string& name) { m_config.name = name; }
    void SetFrameCount(uint32_t frameCount) { m_frameCount = frameCount; }
    void SetWidth(uint32_t width) { m_width = width; }
    void SetHeight(uint32_t height) { m_height = height; }

private:
    // Member değişkenler
    VulkanRenderer* m_renderer = nullptr;
    VulkanDevice* m_device = nullptr;
    BaseConfig m_config;
    bool m_isEnabled = true;
    std::string m_lastError;

    // Vulkan kaynakları
    std::unique_ptr<VulkanShader> m_vertexShader;
    std::unique_ptr<VulkanShader> m_fragmentShader;
    std::unique_ptr<VulkanPipeline> m_pipeline;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    
    // Uniform buffer'lar
    std::vector<std::unique_ptr<VulkanBuffer>> m_uniformBuffers;
    
    // --- MODIFY static vertex buffer to use std::shared_ptr for automatic reference counting ---
    static std::shared_ptr<class VulkanBuffer> s_vertexBuffer;
    static uint32_t s_vertexCount;
    // static uint32_t s_referenceCount; // No longer needed
    
    // Durum yönetimi
    bool m_isInitialized = false;
    uint32_t m_frameCount = 3; // Swapchain frame sayısı
    uint32_t m_width = 1920;    // Varsayılan genişlik
    uint32_t m_height = 1080;   // Varsayılan yükseklik
};

} // namespace AstralEngine