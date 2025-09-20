#pragma once

#include "IRenderer.h"
#include "Core/Engine.h"
#include "Camera.h"
#include "Subsystems/ECS/ECSSubsystem.h"
#include "Material/Material.h"
#include "Core/VulkanFramebuffer.h"
#include "RenderSubsystem.h" // For MeshMaterialKey
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <glm/glm.hpp>

// Forward declarations
namespace AstralEngine {
class GraphicsDevice;
class VulkanShader;
class VulkanPipeline;
class VulkanBuffer;
class VulkanMesh;
class VulkanTexture;
class RenderSubsystem;

class VulkanRenderer : public IRenderer {
public>
    struct ResolvedRenderItem {
        glm::mat4 transform;
        std::shared_ptr<VulkanMesh> mesh;
        std::shared_ptr<Material> material;
    };

    VulkanRenderer();
    virtual ~VulkanRenderer();

    bool Initialize(GraphicsDevice* device, void* owner = nullptr) override;
    void Shutdown() override;

    void SetCamera(Camera* camera) { m_camera = camera; }
    
    // Command recording for different passes
    void RecordShadowPassCommands(VulkanFramebuffer* shadowFramebuffer, const glm::mat4& lightSpaceMatrix, const std::vector<ResolvedRenderItem>& renderItems);
    void RecordGBufferCommands(uint32_t frameIndex, VulkanFramebuffer* gBuffer, const std::map<MeshMaterialKey, std::vector<glm::mat4>>& renderQueue);
    void RecordLightingCommands(uint32_t frameIndex, VulkanFramebuffer* sceneFramebuffer);
    
    // Instance buffer management
    void ResetInstanceBuffer();

    VkRenderPass GetGBufferRenderPass() const { return m_gBufferRenderPass; }
    VkRenderPass GetLightingRenderPass() const { return m_lightingRenderPass; }
    VkRenderPass GetShadowRenderPass() const { return m_shadowRenderPass; }

    // Unused interface methods are omitted for brevity
    void BeginFrame() override {};
    void EndFrame() override;
    void Present() override {};
    void Submit(const RenderCommand& command) override {};
    void SubmitCommands(const std::vector<RenderCommand>& commands) override {};
    bool IsInitialized() const override { return m_isInitialized; };
    RendererAPI GetAPI() const override { return RendererAPI::Vulkan; };
    void SetClearColor(float r, float g, float b, float a) override {};
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override {};

private:
    bool InitializeRenderingComponents();
    void ShutdownRenderingComponents();
    
    void CreateGBufferRenderPass();
    void CreateLightingPass();
    void CreateShadowPass();

    // Pipeline cache management
    std::shared_ptr<VulkanPipeline> GetOrCreatePipeline(const Material& material);
    
    /**
     * @brief Merkezi layout yönetimi metodları
     *
     * Bu metodlar, shader kombinasyonlarına göre layout'ları oluşturur veya cache'den alır.
     * Aynı shader kombinasyonu için aynı layout'un paylaşılmasını sağlarlar.
     */
    
    /**
     * @brief Shader özelliklerinden descriptor set layout oluşturur veya cache'den alır
     *
     * Bu metot, verilen materyalin shader handle'larını kullanarak bir hash oluşturur.
     * Bu hash değerini cache'de arar, eğer varsa mevcut layout'u döndürür.
     * Yoksa yeni bir VkDescriptorSetLayout oluşturur, cache'e ekler ve döndürür.
     *
     * @param material Layout'u oluşturulacak materyal
     * @return VkDescriptorSetLayout Oluşturulan veya cache'den alınan descriptor set layout
     *
     * @par Performans Avantajları
     * - Aynı shader kombinasyonu için layout paylaşımı
     * - Tekrar tekrar layout oluşturma maliyetinin önlenmesi
     * - Bellek kullanımının optimizasyonu
     */
    VkDescriptorSetLayout GetOrCreateDescriptorSetLayout(const Material& material);
    
    /**
     * @brief Descriptor set layout hash'inden pipeline layout oluşturur veya cache'den alır
     *
     * Bu metot, verilen descriptor set layout pointer'ını kullanarak bir hash oluşturur.
     * Bu hash değerini cache'de arar, eğer varsa mevcut pipeline layout'u döndürür.
     * Yoksa yeni bir VkPipelineLayout oluşturur, cache'e ekler ve döndürür.
     *
     * @param descriptorSetLayout Pipeline layout oluşturmak için kullanılacak descriptor set layout
     * @return VkPipelineLayout Oluşturulan veya cache'den alınan pipeline layout
     *
     * @par Performans Avantajları
     * - Aynı descriptor set layout için pipeline layout paylaşımı
     * - Pipeline layout oluşturma maliyetinin azaltılması
     * - Vulkan pipeline oluşturma sürecinin hızlandırılması
     */
    VkPipelineLayout GetOrCreatePipelineLayout(VkDescriptorSetLayout descriptorSetLayout);

    GraphicsDevice* m_graphicsDevice = nullptr;
    RenderSubsystem* m_renderSubsystem = nullptr;
    Engine* m_owner = nullptr;
    
    bool m_isInitialized = false;
    Camera* m_camera = nullptr;
    
    // Pass Resources
    VkRenderPass m_gBufferRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_lightingRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_shadowRenderPass = VK_NULL_HANDLE;

    std::shared_ptr<VulkanPipeline> m_lightingPipeline;
    VkDescriptorSetLayout m_lightingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_lightingDescriptorSet = VK_NULL_HANDLE;

    std::shared_ptr<VulkanPipeline> m_shadowPipeline;
    
    // Pipeline cache for material-based pipeline management
    std::map<size_t, std::shared_ptr<VulkanPipeline>> m_pipelineCache;
    
    /**
     * @defgroup MerkeziLayoutYonetimi Merkezi Layout Yönetim Sistemi
     * @brief VulkanRenderer'da descriptor set ve pipeline layout'larını merkezi olarak yöneten sistem
     *
     * Bu sistem, aynı shader kombinasyonunu kullanan materyaller arasında layout paylaşımını sağlar.
     * Bu sayede:
     * - Bellek kullanımı optimize edilir (aynı layout için tekrar tekrar oluşturma önlenir)
     * - Performans artar (layout oluşturma maliyeti azalır)
     * - Material sınıfı lightweight bir veri konteyneri olarak kalabilir
     * - Vulkan kaynakları merkezi olarak yönetilir ve temizlenir
     *
     * @par Cache Mekanizması
     * - Shader handle'larından benzersiz hash değerleri oluşturulur
     * - Aynı hash değerine sahip istekler cache'den karşılanır
     * - Cache'de bulunmayan layout'lar oluşturulur ve cache'e eklenir
     * - Shutdown() sırasında tüm cache kaynakları düzgün şekilde temizlenir
     */
    
    // Aynı shader kombinasyonu için descriptor set layout'ları paylaşır
    std::unordered_map<size_t, VkDescriptorSetLayout> m_descriptorSetLayoutCache;
    // Aynı shader kombinasyonu için pipeline layout'ları paylaşır
    std::unordered_map<size_t, VkPipelineLayout> m_pipelineLayoutCache;
    
    // Frame-ringed instance buffer for performance optimization
    std::vector<std::unique_ptr<VulkanBuffer>> m_instanceBuffers;
    void* m_instanceBufferMapped = nullptr;
    uint32_t m_instanceBufferOffset = 0;
    static const uint32_t INSTANCE_BUFFER_SIZE = 1024 * 1024; // 1MB, adjust as needed
};

} // namespace AstralEngine
