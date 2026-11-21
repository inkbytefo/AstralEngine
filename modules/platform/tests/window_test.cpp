#include <astral/platform/input.hpp>
#include <astral/platform/window.hpp>
#include <doctest/doctest.h>

#include <cstdlib>

using namespace astral;

namespace {
// Tests run without a display: SDL's dummy video driver gives real windows with no output.
std::unique_ptr<Window> make_window() {
    setenv("SDL_VIDEO_DRIVER", "dummy", 0);
    auto window = Window::create({.title = "test", .width = 320, .height = 200, .hidden = true});
    if (!window) MESSAGE("no video driver available — skipping");
    return window;
}
} // namespace

TEST_CASE("window reports its size and keeps running until closed") {
    auto window = make_window();
    if (!window) return;
    Input input;
    CHECK(window->poll(input));
    CHECK(window->size().width == 320);
    CHECK(window->size().height == 200);
    window->request_close();
    CHECK_FALSE(window->poll(input));
}

TEST_CASE("window can lend itself to the gpu as a surface source") {
    auto window = make_window();
    if (!window) return;
    const gpu::SurfaceSource source = window->surface_source();
    CHECK(static_cast<bool>(source.create_surface) == window->supports_vulkan());
    CHECK(source.drawable_size().width == 320);
}
