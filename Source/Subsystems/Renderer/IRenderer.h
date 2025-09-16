#pragma once

#include "RendererTypes.h"
#include <memory>
#include <vector>

namespace AstralEngine {

// Forward declaration
class GraphicsDevice;

/**
 * @interface IRenderer
 * @brief Abstract interface for rendering backends
 * 
 * This interface defines the common functionality that all rendering
 * implementations must provide, allowing for easy swapping between
 * different graphics APIs (Vulkan, DirectX, OpenGL, etc.)
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Core lifecycle
    virtual bool Initialize(GraphicsDevice* device, void* owner = nullptr) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Frame management
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;

    // Command submission
    virtual void Submit(const RenderCommand& command) = 0;
    virtual void SubmitCommands(const std::vector<RenderCommand>& commands) = 0;

    // Configuration
    virtual void SetClearColor(float r, float g, float b, float a) = 0;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

    // Information
    virtual RendererAPI GetAPI() const = 0;
};

} // namespace AstralEngine
