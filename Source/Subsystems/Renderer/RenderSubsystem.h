#pragma once

#include "../../Core/ISubsystem.h"
#include "GraphicsDevice.h"
#include "Camera.h"
#include <memory>

namespace AstralEngine {

class Window;

/**
 * @brief Vulkan tabanlı render alt sistemi
 * 
 * Grafik API'sini (Vulkan) soyutlar ve sahneyi ekrana çizer.
 * SDL3 window ve GraphicsDevice ile entegre çalışır.
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

private:
    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    std::unique_ptr<Camera> m_camera;
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
};

} // namespace AstralEngine
