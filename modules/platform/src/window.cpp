#include <astral/platform/input.hpp>
#include <astral/platform/window.hpp>

#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <string>

namespace astral {

namespace {

Key to_key(SDL_Scancode code) {
    if (code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z) {
        return static_cast<Key>(static_cast<int>(Key::A) + (code - SDL_SCANCODE_A));
    }
    if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9) {
        return static_cast<Key>(static_cast<int>(Key::Num1) + (code - SDL_SCANCODE_1));
    }
    if (code >= SDL_SCANCODE_F1 && code <= SDL_SCANCODE_F12) {
        return static_cast<Key>(static_cast<int>(Key::F1) + (code - SDL_SCANCODE_F1));
    }
    switch (code) {
    case SDL_SCANCODE_0: return Key::Num0;
    case SDL_SCANCODE_SPACE: return Key::Space;
    case SDL_SCANCODE_RETURN: return Key::Enter;
    case SDL_SCANCODE_ESCAPE: return Key::Escape;
    case SDL_SCANCODE_TAB: return Key::Tab;
    case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
    case SDL_SCANCODE_LEFT: return Key::Left;
    case SDL_SCANCODE_RIGHT: return Key::Right;
    case SDL_SCANCODE_UP: return Key::Up;
    case SDL_SCANCODE_DOWN: return Key::Down;
    case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
    case SDL_SCANCODE_RSHIFT: return Key::RightShift;
    case SDL_SCANCODE_LCTRL: return Key::LeftCtrl;
    case SDL_SCANCODE_RCTRL: return Key::RightCtrl;
    case SDL_SCANCODE_LALT: return Key::LeftAlt;
    case SDL_SCANCODE_RALT: return Key::RightAlt;
    default: return Key::Unknown;
    }
}

MouseButton to_button(Uint8 button) {
    switch (button) {
    case SDL_BUTTON_RIGHT: return MouseButton::Right;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    default: return MouseButton::Left;
    }
}

} // namespace

struct Window::Impl {
    SDL_Window* window = nullptr;
    bool vulkan = true; ///< False when the video driver cannot create Vulkan surfaces.
    ~Impl() {
        if (window) SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO); // reference counted by SDL
    }
};

std::unique_ptr<Window> Window::create(const WindowDesc& desc) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return nullptr;
    auto impl = std::make_unique<Impl>();
    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (desc.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (desc.hidden) flags |= SDL_WINDOW_HIDDEN;
    const std::string title(desc.title);
    impl->window = SDL_CreateWindow(title.c_str(), static_cast<int>(desc.width),
                                    static_cast<int>(desc.height), flags);
    if (!impl->window) {
        // Some drivers (e.g. headless dummy) cannot do Vulkan windows; retry without it.
        impl->window = SDL_CreateWindow(title.c_str(), static_cast<int>(desc.width),
                                        static_cast<int>(desc.height), flags & ~SDL_WINDOW_VULKAN);
        impl->vulkan = false;
    }
    if (!impl->window) return nullptr;
    return std::unique_ptr<Window>(new Window(std::move(impl)));
}

Window::Window(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Window::~Window() = default;

bool Window::poll(Input& input) {
    input.begin_frame();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: close_requested_ = true; break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            input.set_key(to_key(event.key.scancode), event.type == SDL_EVENT_KEY_DOWN);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            input.set_mouse_button(to_button(event.button.button),
                                   event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            input.add_mouse_motion({event.motion.x, event.motion.y},
                                   {event.motion.xrel, event.motion.yrel});
            break;
        case SDL_EVENT_MOUSE_WHEEL: input.add_scroll({event.wheel.x, event.wheel.y}); break;
        default: break;
        }
    }
    return !close_requested_;
}

gpu::Extent2D Window::size() const {
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(impl_->window, &w, &h);
    return {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)};
}

bool Window::minimized() const {
    return (SDL_GetWindowFlags(impl_->window) & SDL_WINDOW_MINIMIZED) != 0;
}

void Window::set_title(std::string_view title) {
    const std::string copy(title);
    SDL_SetWindowTitle(impl_->window, copy.c_str());
}

void Window::set_mouse_captured(bool captured) {
    SDL_SetWindowRelativeMouseMode(impl_->window, captured);
}

gpu::SurfaceSource Window::surface_source() const {
    gpu::SurfaceSource source;
    source.drawable_size = [this] { return size(); };
    if (!impl_->vulkan) return source; // headless/dummy driver: no presentation possible
    Uint32 count = 0;
    if (const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count)) {
        for (Uint32 i = 0; i < count; ++i) source.instance_extensions.emplace_back(names[i]);
    }
    SDL_Window* window = impl_->window;
    source.create_surface = [window](void* instance) -> std::uint64_t {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(window, static_cast<VkInstance>(instance), nullptr, &surface)) {
            return 0;
        }
        return reinterpret_cast<std::uint64_t>(surface);
    };
    return source;
}

bool Window::supports_vulkan() const noexcept { return impl_->vulkan; }

} // namespace astral
