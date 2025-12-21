#pragma once

#include "../../Core/ISubsystem.h"
#include "GraphicsDevice.h"
#include "Camera.h"
#include "../Asset/AssetSubsystem.h"
#include "VulkanMeshManager.h"
#include "VulkanTextureManager.h"
#include "Material/Material.h"
#include "Core/VulkanFramebuffer.h"
#include <glm/glm.hpp>

// Forward declarations
class PostProcessingSubsystem;
class TonemappingEffect;
class BloomEffect;
class VulkanRenderer;

#include <memory>
#include <vector>
#include <map>

// UI Subsystem forward declaration
#ifdef ASTRAL_USE_IMGUI
    #include <vulkan/vulkan.h>
    class UISubsystem;
#endif

namespace AstralEngine {

class Window;
class ECSSubsystem;
class AssetSubsystem;
class VulkanMeshManager;
class VulkanTextureManager;
class VulkanBuffer;
class VulkanTexture;
class MaterialManager;
class Camera;

static constexpr int LIGHT_TYPE_DIRECTIONAL = 0;
static constexpr int LIGHT_TYPE_POINT = 1;
static constexpr int LIGHT_TYPE_SPOT = 2;

struct alignas(16) GPULight {
    glm::mat4 lightSpaceMatrix{1.0f};
    glm::vec3 position{0.0f};
    int type = LIGHT_TYPE_POINT;
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    float range = 0.0f;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float constant = 1.0f;
    float linear = 0.0f;
    float quadratic = 0.0f;
    float innerConeCos = 0.0f;
    float outerConeCos = 0.0f;
    float castsShadows = 0.0f;
    float _padding0 = 0.0f;
    float _padding1 = 0.0f;
};

static_assert(sizeof(GPULight) % 16 == 0, "GPULight must be 16-byte aligned");

class RenderSubsystem : public ISubsystem {
public:
    RenderSubsystem();
    ~RenderSubsystem() override;

    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "RenderSubsystem"; }
    UpdateStage GetUpdateStage() const override { return UpdateStage::Render; }

    // Getters
    Engine* GetOwner() const { return m_owner; }
    GraphicsDevice* GetGraphicsDevice() const { return m_graphicsDevice.get(); }
    VulkanTexture* GetSceneColorTexture() const { return m_sceneColorTexture.get(); }
    VulkanBuffer* GetSceneUBO() const { return m_sceneUBO.get(); }
    VulkanTexture* GetShadowMapTexture() const { return m_shadowMapTexture.get(); }
    MaterialManager* GetMaterialManager() const { return m_materialManager.get(); }
    VulkanMeshManager* GetVulkanMeshManager() const { return m_vulkanMeshManager.get(); }
    VulkanTextureManager* GetVulkanTextureManager() const { return m_vulkanTextureManager.get(); }
    Camera* GetCamera() const { return m_camera.get(); }


    // G-Buffer textures are now accessed through the GBufferPass
    VulkanTexture* GetAlbedoTexture() const;
    VulkanTexture* GetNormalTexture() const;
    VulkanTexture* GetPBRTexture() const;
    VulkanTexture* GetDepthTexture() const;
    // G-Buffer framebuffer access
    VulkanFramebuffer* GetGBufferFramebuffer() const;
    
    // Post-processing integration
    void SetPostProcessingInputTexture(VulkanTexture* sceneColorTexture);
    PostProcessingSubsystem* GetPostProcessingSubsystem() const { return m_postProcessing.get(); }
    
    // VulkanRenderer connection
    void SetVulkanRenderer(VulkanRenderer* renderer);
    
    // Swapchain blit
    void BlitToSwapchain(VkCommandBuffer commandBuffer, VulkanTexture* sourceTexture);

    // UI Integration
    void RenderUI();
    VkRenderPass GetUIRenderPass() const;
    VkCommandBuffer GetCurrentUICommandBuffer() const;

private:
    // Pass-related private methods are now removed or moved
    void CreateLightingPassResources();
    void DestroyLightingPassResources();
    void CreateShadowPassResources();
    void DestroyShadowPassResources();
    void CreateSceneColorTexture(uint32_t width, uint32_t height);
    void DestroySceneColorTexture();

    // High-level pass execution logic
    void ShadowPass();
    // GBufferPass() is now handled by GBufferPass class
    void LightingPass();
    void UpdateLightsAndShadows();

    bool CheckAssetReadiness(const AssetHandle& modelHandle, const AssetHandle& materialHandle) const;
    
    // G-Buffer validation
    void EnsureGBufferPassInitialized();

    void CreateUIRenderPass();
    void CreateUIFramebuffers();
    void CreateUICommandBuffers();
    void DestroyUIResources();

    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    ECSSubsystem* m_ecsSubsystem = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;

    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<MaterialManager> m_materialManager;
    std::unique_ptr<VulkanMeshManager> m_vulkanMeshManager;
    std::unique_ptr<VulkanTextureManager> m_vulkanTextureManager;

    std::unique_ptr<VulkanTexture> m_gbAlbedoTexture;
    std::unique_ptr<VulkanTexture> m_gbNormalTexture;
    std::unique_ptr<VulkanTexture> m_gbPBRTexture;
    std::unique_ptr<VulkanTexture> m_gbDepthTexture;

    // Lighting Pass Resources
    std::unique_ptr<VulkanFramebuffer> m_sceneFramebuffer;
    std::unique_ptr<VulkanTexture> m_sceneColorTexture;
    std::unique_ptr<VulkanBuffer> m_sceneUBO;
    std::vector<GPULight> m_lights;

    // Shadow Pass Resources
    std::unique_ptr<VulkanFramebuffer> m_shadowFramebuffer;
    std::unique_ptr<VulkanTexture> m_shadowMapTexture;
    glm::mat4 m_lightSpaceMatrix;
    
    // Post-processing için yeni üyeler
    std::unique_ptr<PostProcessingSubsystem> m_postProcessing;

    // UI rendering resources
#ifdef ASTRAL_USE_IMGUI
    VkRenderPass m_uiRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_uiFramebuffers;
    std::vector<VkCommandBuffer> m_uiCommandBuffers;
    std::vector<VkCommandPool> m_uiCommandPools;
    uint32_t m_currentFrame = 0;
#endif
};

} // namespace AstralEngine
