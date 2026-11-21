#include <astral/gpu/swapchain.hpp>
#include <astral/platform/input.hpp>
#include <astral/platform/window.hpp>
#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("a window presents frames through the swapchain") {
    auto window = Window::create({.title = "present test", .width = 256, .height = 128});
    if (!window || !window->supports_vulkan()) {
        MESSAGE("no display with Vulkan support — skipping");
        return;
    }
    const auto surface = window->surface_source();
    auto device = gpu::Device::create({.app_name = "present_test", .validation = true, .surface = &surface});
    if (!device) {
        MESSAGE("no Vulkan device can present to this window — skipping");
        return;
    }
    auto swapchain = gpu::Swapchain::create(*device, surface);
    REQUIRE(swapchain);
    CHECK(swapchain->extent().width == 256);
    CHECK(swapchain->extent().height == 128);

    Input input;
    int presented = 0;
    for (int frame = 0; frame < 5 && window->poll(input); ++frame) {
        gpu::TextureHandle image = swapchain->acquire();
        if (!image.valid()) continue;
        auto cmd = device->begin_commands();
        cmd.transition(image, gpu::ResourceState::ColorTarget);
        cmd.begin_rendering({.color = {{.texture = image, .clear = {0.1f, 0.2f, 0.3f, 1.0f}}}});
        cmd.end_rendering();
        cmd.transition(image, gpu::ResourceState::Present);
        device->submit(cmd);
        swapchain->present();
        ++presented;
    }
    device->wait_idle();
    CHECK(presented == 5);
}
