#include <astral/platform/input.hpp>
#include <astral/platform/window.hpp>
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>

using namespace astral;

namespace {
// Tests run without a display: SDL's dummy video driver gives real windows with no output.
std::unique_ptr<Window> make_window() {
#if defined(_WIN32)
    std::size_t len = 0;
    if (getenv_s(&len, nullptr, 0, "SDL_VIDEO_DRIVER") != 0 || len == 0) _putenv_s("SDL_VIDEO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 0);
#endif
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

TEST_CASE("keeping the window responsive neither fails nor loses a pending close request") {
    auto window = make_window();
    if (!window) return;
    Input input;
    for (int i = 0; i < 100; ++i) window->keep_responsive();
    CHECK(window->poll(input));
    window->request_close();
    window->keep_responsive();
    CHECK_FALSE(window->poll(input));
}

TEST_CASE("window can lend itself to the gpu as a surface source") {
    auto window = make_window();
    if (!window) return;
    const gpu::SurfaceSource source = window->surface_source();
    CHECK(static_cast<bool>(source.create_surface) == window->supports_vulkan());
    CHECK(source.drawable_size().width == 320);
}
