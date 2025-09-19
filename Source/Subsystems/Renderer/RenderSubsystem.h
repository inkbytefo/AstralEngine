#pragma once

#include "../../Core/ISubsystem.h"
#include "GraphicsDevice.h"
#include "Camera.h"
#include "../Asset/AssetSubsystem.h"
#include "VulkanMeshManager.h"
#include "VulkanTextureManager.h"
#include "Material/Material.h"
#include "Core/VulkanFramebuffer.h"
// PostProcessingSubsystem forward declaration
class PostProcessingSubsystem;
#include "../../Assets/Shaders/Include/lighting_structs.slang"
#include <memory>
#include <vector>
#include <map>

namespace AstralEngine {

struct MeshMaterialKey {
    AssetHandle modelHandle;
    AssetHandle materialHandle;

    bool operator<(const MeshMaterialKey& other) const {
        if (modelHandle < other.modelHandle) return true;
        if (other.modelHandle < modelHandle) return false;
        return materialHandle < other.materialHandle;
    }
};

class Window;
class ECSSubsystem;
class AssetSubsystem;
class VulkanMeshManager;
class VulkanTextureManager;
class VulkanBuffer;
class VulkanTexture;

class RenderSubsystem : public ISubsystem {
public>
    RenderSubsystem();
    ~RenderSubsystem() override;

    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "RenderSubsystem"; }

    // Getters are omitted for brevity
    GraphicsDevice* GetGraphicsDevice() const { return m_graphicsDevice.get(); }
    VulkanTexture* GetSceneColorTexture() const { return m_sceneColorTexture.get(); }
    VulkanBuffer* GetSceneUBO() const { return m_sceneUBO.get(); }
    VulkanTexture* GetAlbedoTexture() const { return m_gBufferAlbedo.get(); }
    VulkanTexture* GetNormalTexture() const { return m_gBufferNormal.get(); }
    VulkanTexture* GetPBRTexture() const { return m_gBufferPBR.get(); }
    VulkanTexture* GetDepthTexture() const { return m_gBufferDepth.get(); }
    VulkanTexture* GetShadowMapTexture() const { return m_shadowMapTexture.get(); }
    
    // Post-processing entegrasyonu için yeni metodlar
    void SetPostProcessingInputTexture(VulkanTexture* sceneColorTexture);
    PostProcessingSubsystem* GetPostProcessingSubsystem() const { return m_postProcessing.get(); }
    
    // VulkanRenderer bağlantısı için yeni metod
    void SetVulkanRenderer(VulkanRenderer* renderer);
    
    // Swapchain'e blit işlemi için metod
    void BlitToSwapchain(VkCommandBuffer commandBuffer, VulkanTexture* sourceTexture);

private:
    void CreateGBuffer();
    void DestroyGBuffer();
    void CreateLightingPassResources();
    void DestroyLightingPassResources();
    void CreateShadowPassResources();
    void DestroyShadowPassResources();
    void CreateSceneColorTexture(uint32_t width, uint32_t height);
    void DestroySceneColorTexture();

    void ShadowPass();
    void GBufferPass();
    void LightingPass();
    void UpdateLightsAndShadows();

    bool CheckAssetReadiness(const AssetHandle& modelHandle, const AssetHandle& materialHandle) const;

    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    ECSSubsystem* m_ecsSubsystem = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;

    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<MaterialManager> m_materialManager;
    std::unique_ptr<VulkanMeshManager> m_vulkanMeshManager;
    std::unique_ptr<VulkanTextureManager> m_vulkanTextureManager;

    // G-Buffer Resources
    std::unique_ptr<VulkanFramebuffer> m_gBufferFramebuffer;
    std::unique_ptr<VulkanTexture> m_gBufferAlbedo, m_gBufferNormal, m_gBufferPBR, m_gBufferDepth;

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
};

} // namespace AstralEngine
