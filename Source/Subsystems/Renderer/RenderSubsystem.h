#pragma once

#include "../../Core/ISubsystem.h"
#include "GraphicsDevice.h"
#include "Camera.h"
#include "../Asset/AssetManager.h"
#include "../Material/Material.h"
#include "../Texture/TextureManager.h"
#include "../Shader/ShaderManager.h"
#include <memory>
#include <unordered_map>

namespace AstralEngine {

class Window;
class ECSSubsystem;

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
    AssetManager* GetAssetManager() const { return m_assetManager.get(); }
    MaterialManager* GetMaterialManager() const { return m_materialManager.get(); }
    TextureManager* GetTextureManager() const { return m_textureManager.get(); }
    ShaderManager* GetShaderManager() const { return m_shaderManager.get(); }

private:
    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<AssetManager> m_assetManager;
    std::unique_ptr<MaterialManager> m_materialManager;
    std::unique_ptr<TextureManager> m_textureManager;
    std::unique_ptr<ShaderManager> m_shaderManager;
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    ECSSubsystem* m_ecsSubsystem = nullptr;
};

} // namespace AstralEngine
