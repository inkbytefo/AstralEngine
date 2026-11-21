// hello_triangle — the lowest layer of AstralEngine used directly: a window, a GPU device,
// a swapchain and one pipeline. Games normally use Astral::App instead (see docs, ADR-0006).
#include <astral/core/clock.hpp>
#include <astral/core/log.hpp>
#include <astral/gpu/swapchain.hpp>
#include <astral/platform/input.hpp>
#include <astral/platform/window.hpp>

#include <shaders/triangle.frag.hpp>
#include <shaders/triangle.vert.hpp>

#include <memory>

int main() {
    astral::Logger log;
    log.add_sink(std::make_shared<astral::ConsoleSink>());

    auto window = astral::Window::create({.title = "Astral — hello triangle"});
    if (!window) {
        log.error("could not create a window");
        return 1;
    }
    const auto surface = window->surface_source();
    auto device = astral::gpu::Device::create(
        {.app_name = "hello_triangle", .validation = true, .surface = &surface, .logger = &log});
    if (!device) return 1;
    log.info("GPU: {}", device->adapter_name());

    auto swapchain = astral::gpu::Swapchain::create(*device, surface);
    auto pipeline = device->create_graphics_pipeline({
        .vertex = {astral::shaders::triangle_vert},
        .fragment = {astral::shaders::triangle_frag},
        .color_formats = {swapchain->format()},
    });

    astral::Input input;
    astral::Clock clock;
    float time = 0.0f;
    while (window->poll(input)) {
        if (input.pressed(astral::Key::Escape)) window->request_close();
        time += clock.tick();

        astral::gpu::TextureHandle image = swapchain->acquire();
        if (!image.valid()) continue; // minimized or resizing

        auto cmd = device->begin_commands();
        cmd.transition(image, astral::gpu::ResourceState::ColorTarget);
        cmd.begin_rendering({.color = {{.texture = image, .clear = {0.05f, 0.05f, 0.08f, 1.0f}}}});
        cmd.bind_pipeline(pipeline);
        cmd.push_constants(&time, sizeof(time));
        cmd.draw(3);
        cmd.end_rendering();
        cmd.transition(image, astral::gpu::ResourceState::Present);
        device->submit(cmd);
        swapchain->present();
    }
    device->wait_idle();
}
