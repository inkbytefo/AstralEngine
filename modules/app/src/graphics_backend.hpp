#pragma once
// Internal to Astral::App: owns the display and GPU stack in strict reverse-teardown order.

#include <astral/gpu/device.hpp>

#include <memory>

namespace astral {

struct AppDesc;
class Logger;
class Renderer;
class Window;
namespace gpu {
class Swapchain;
}

/// Initializes and tears down Window, SurfaceSource, Device, Swapchain, and Renderer.
class GraphicsBackend {
public:
    GraphicsBackend();
    ~GraphicsBackend();
    GraphicsBackend(const GraphicsBackend&) = delete;
    GraphicsBackend& operator=(const GraphicsBackend&) = delete;

    void init(const AppDesc& desc, Logger& log);
    void wait_idle();
    void shutdown();

    [[nodiscard]] Window* window() noexcept { return window_.get(); }
    [[nodiscard]] gpu::Device* device() noexcept { return device_.get(); }
    [[nodiscard]] gpu::Swapchain* swapchain() noexcept { return swapchain_.get(); }
    [[nodiscard]] Renderer* renderer() noexcept { return renderer_.get(); }

private:
    std::unique_ptr<Window> window_;
    gpu::SurfaceSource surface_;
    std::unique_ptr<gpu::Device> device_;
    std::unique_ptr<gpu::Swapchain> swapchain_;
    std::unique_ptr<Renderer> renderer_;
};

} // namespace astral
