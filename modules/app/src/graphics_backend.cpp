#include "graphics_backend.hpp"

#include <astral/app/app.hpp>
#include <astral/core/log.hpp>
#include <astral/gpu/swapchain.hpp>
#include <astral/platform/window.hpp>
#include <astral/render/renderer.hpp>

namespace astral {

GraphicsBackend::GraphicsBackend() = default;
GraphicsBackend::~GraphicsBackend() { shutdown(); }

void GraphicsBackend::init(const AppDesc& desc, Logger& log) {
    if (desc.headless) return;
    if (desc.offscreen) {
        device_ = gpu::Device::create({.app_name = desc.title, .validation = desc.validation, .logger = &log});
        if (device_) renderer_ = std::make_unique<Renderer>(*device_, gpu::Format::RGBA8Unorm, desc.render);
        else log.error("App: no usable GPU for offscreen rendering; running without rendering");
        return;
    }
    window_ = Window::create({.title = desc.title, .width = desc.width, .height = desc.height});
    if (!window_) {
        log.error("App: could not create a window; running headless");
        return;
    }
    surface_ = window_->surface_source();
    device_ = gpu::Device::create(
        {.app_name = desc.title, .validation = desc.validation, .surface = &surface_, .logger = &log});
    if (!device_) {
        log.error("App: no usable GPU; running without rendering");
        return;
    }
    swapchain_ = gpu::Swapchain::create(*device_, surface_, {.vsync = desc.vsync});
    if (swapchain_) renderer_ = std::make_unique<Renderer>(*device_, swapchain_->format(), desc.render);
}

void GraphicsBackend::wait_idle() {
    if (device_) device_->wait_idle();
}

void GraphicsBackend::shutdown() {
    renderer_.reset();
    swapchain_.reset();
    device_.reset();
    window_.reset();
}

} // namespace astral
