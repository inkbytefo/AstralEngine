# /**********************************************************************
#  * Astral Engine v0.1.0-alpha - Rendering Subsystem
#  *
#  * Header File: RenderSubsystem.h
#  * Purpose: High-level subsystem for managing rendering operations.
#  * Author: Jules (AI Assistant)
#  * Version: 1.0.0
#  * Date: 2025-09-15
#  * Dependencies: C++20, ISubsystem, GraphicsDevice
#  **********************************************************************/
#pragma once

#include "../../Core/ISubsystem.h"
#include "GraphicsDevice.h"
#include <memory>

namespace AstralEngine {

class Engine;
class Window;

/**
 * @brief The primary rendering subsystem for the engine.
 *
 * This subsystem orchestrates the rendering process. It owns the
 * GraphicsDevice and manages the high-level render loop, but delegates
 * low-level API calls.
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

    // High-level rendering commands
    uint32_t BeginFrame();
    void EndFrame(uint32_t imageIndex);

    // Accessors
    GraphicsDevice* GetGraphicsDevice() const { return m_graphicsDevice.get(); }

private:
    void CreateSyncObjects();

    std::unique_ptr<GraphicsDevice> m_graphicsDevice;
    Engine* m_owner = nullptr;
    Window* m_window = nullptr;
    bool m_isInitialized = false;

    // Synchronization
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
};

} // namespace AstralEngine
