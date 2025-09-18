#pragma once

#include "../../Core/ISubsystem.h"
#include "GraphicsDevice.h"
#include "Camera.h"
#include "../Asset/AssetSubsystem.h"
#include "VulkanMeshManager.h"
#include "VulkanTextureManager.h"
#include "../Material/Material.h"
#include <memory>
#include <unordered_map>

namespace AstralEngine {

class Window;
class ECSSubsystem;
class AssetSubsystem;
class VulkanMeshManager;
class VulkanTextureManager;

/**
 * @brief Vulkan tabanlı render alt sistemi
 * 
 * Grafik API'sini (Vulkan) soyutlar ve sahneyi ekrana çizer.
 * SDL3 window ve GraphicsDevice ile entegre çalışır.
 * ECSSubsystem'den gelen render verilerini kullanarak çizim yapar.
 */
class RenderSubsystem : public ISubsystem {
public:
    RenderSubsystem();
    ~RenderSubsystem() override;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "RenderSubsystem"; }

    // Render interface
    void BeginFrame();
    void EndFrame();

    // Getters
    GraphicsDevice* GetGraphicsDevice() const { return m_graphicsDevice.get(); }
    Camera* GetCamera() const { return m_camera.get(); }
    MaterialManager* GetMaterialManager() const { return m_materialManager.get(); }
    VulkanMeshManager* GetVulkanMeshManager() const { return m_vulkanMeshManager.get(); }
    VulkanTextureManager* GetVulkanTextureManager() const { return m_vulkanTextureManager.get(); }
    AssetSubsystem* GetAssetSubsystem() const { return m_assetSubsystem; }

    // Material readiness check
    bool IsMaterialReady(const std::shared_ptr<Material>& material) const;

    // Asset readiness check for async loading
    bool CheckAssetReadiness(const AssetHandle& modelHandle, const AssetHandle& materialHandle) const;
    
    // Debug logging for asset status
    void LogAssetStatus() const;

private:
    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<MaterialManager> m_materialManager;
    std::unique_ptr<VulkanMeshManager> m_vulkanMeshManager;
    std::unique_ptr<VulkanTextureManager> m_vulkanTextureManager;
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    ECSSubsystem* m_ecsSubsystem = nullptr;
    AssetSubsystem* m_assetSubsystem = nullptr;
    
    // Async loading configuration
    bool m_enableAsyncLoading = true;
    
    // Debug statistics
    mutable uint32_t m_framesProcessed = 0;
    mutable uint32_t m_meshesReady = 0;
    mutable uint32_t m_texturesReady = 0;
    mutable uint32_t m_meshesPending = 0;
    mutable uint32_t m_texturesPending = 0;
};

} // namespace AstralEngine
