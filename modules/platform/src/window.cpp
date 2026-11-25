#include <astral/platform/input.hpp>
#include <astral/platform/window.hpp>

#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <optional>
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
    case SDL_SCANCODE_DELETE: return Key::Delete;
    case SDL_SCANCODE_INSERT: return Key::Insert;
    case SDL_SCANCODE_HOME: return Key::Home;
    case SDL_SCANCODE_END: return Key::End;
    case SDL_SCANCODE_PAGEUP: return Key::PageUp;
    case SDL_SCANCODE_PAGEDOWN: return Key::PageDown;
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
    case SDL_SCANCODE_LGUI: return Key::LeftSuper;
    case SDL_SCANCODE_RGUI: return Key::RightSuper;
    case SDL_SCANCODE_MINUS: return Key::Minus;
    case SDL_SCANCODE_EQUALS: return Key::Equal;
    case SDL_SCANCODE_LEFTBRACKET: return Key::LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET: return Key::RightBracket;
    case SDL_SCANCODE_BACKSLASH: return Key::Backslash;
    case SDL_SCANCODE_SEMICOLON: return Key::Semicolon;
    case SDL_SCANCODE_APOSTROPHE: return Key::Apostrophe;
    case SDL_SCANCODE_GRAVE: return Key::Grave;
    case SDL_SCANCODE_COMMA: return Key::Comma;
    case SDL_SCANCODE_PERIOD: return Key::Period;
    case SDL_SCANCODE_SLASH: return Key::Slash;
    default: return Key::Unknown;
    }
}

bool to_pad_button(Uint8 button, GamepadButton& out) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: out = GamepadButton::A; return true;
    case SDL_GAMEPAD_BUTTON_EAST: out = GamepadButton::B; return true;
    case SDL_GAMEPAD_BUTTON_WEST: out = GamepadButton::X; return true;
    case SDL_GAMEPAD_BUTTON_NORTH: out = GamepadButton::Y; return true;
    case SDL_GAMEPAD_BUTTON_BACK: out = GamepadButton::Back; return true;
    case SDL_GAMEPAD_BUTTON_START: out = GamepadButton::Start; return true;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: out = GamepadButton::LeftStick; return true;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: out = GamepadButton::RightStick; return true;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: out = GamepadButton::LeftShoulder; return true;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: out = GamepadButton::RightShoulder; return true;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: out = GamepadButton::DpadUp; return true;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: out = GamepadButton::DpadDown; return true;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: out = GamepadButton::DpadLeft; return true;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: out = GamepadButton::DpadRight; return true;
    default: return false;
    }
}

/// Left, right and middle only; other buttons (X1/X2, ...) have no MouseButton and are ignored.
std::optional<MouseButton> to_button(Uint8 button) {
    switch (button) {
    case SDL_BUTTON_LEFT: return MouseButton::Left;
    case SDL_BUTTON_RIGHT: return MouseButton::Right;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    default: return std::nullopt;
    }
}

} // namespace

struct Window::Impl {
    SDL_Window* window = nullptr;
    SDL_Gamepad* gamepad = nullptr; ///< The first connected gamepad.
    std::array<SDL_Cursor*, static_cast<std::size_t>(CursorShape::Count)> cursors{};
    CursorShape active_cursor = CursorShape::Arrow;
    bool vulkan = true; ///< False when the video driver cannot create Vulkan surfaces.
    ~Impl() {
        for (SDL_Cursor* c : cursors) {
            if (c) SDL_DestroyCursor(c);
        }
        if (gamepad) SDL_CloseGamepad(gamepad);
        if (window) SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        SDL_QuitSubSystem(SDL_INIT_VIDEO); // reference counted by SDL
    }
};

std::unique_ptr<Window> Window::create(const WindowDesc& desc) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return nullptr;
    SDL_InitSubSystem(SDL_INIT_GAMEPAD); // optional: without it there are no gamepad events
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
            if (const auto button = to_button(event.button.button)) {
                input.set_mouse_button(*button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            input.add_mouse_motion({event.motion.x, event.motion.y},
                                   {event.motion.xrel, event.motion.yrel});
            break;
        case SDL_EVENT_MOUSE_WHEEL: input.add_scroll({event.wheel.x, event.wheel.y}); break;
        case SDL_EVENT_TEXT_INPUT: input.add_text(event.text.text); break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!impl_->gamepad) {
                impl_->gamepad = SDL_OpenGamepad(event.gdevice.which);
                input.set_gamepad_connected(impl_->gamepad != nullptr);
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (impl_->gamepad && SDL_GetGamepadID(impl_->gamepad) == event.gdevice.which) {
                SDL_CloseGamepad(impl_->gamepad);
                impl_->gamepad = nullptr;
                input.set_gamepad_connected(false);
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            GamepadButton b;
            if (to_pad_button(event.gbutton.button, b)) input.set_gamepad_button(b, event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            const float v = static_cast<float>(event.gaxis.value) / 32767.0f;
            switch (event.gaxis.axis) {
            case SDL_GAMEPAD_AXIS_LEFTX: input.set_gamepad_axis(GamepadAxis::LeftX, v); break;
            case SDL_GAMEPAD_AXIS_LEFTY: input.set_gamepad_axis(GamepadAxis::LeftY, -v); break; // up is positive
            case SDL_GAMEPAD_AXIS_RIGHTX: input.set_gamepad_axis(GamepadAxis::RightX, v); break;
            case SDL_GAMEPAD_AXIS_RIGHTY: input.set_gamepad_axis(GamepadAxis::RightY, -v); break;
            case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: input.set_gamepad_axis(GamepadAxis::LeftTrigger, v); break;
            case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: input.set_gamepad_axis(GamepadAxis::RightTrigger, v); break;
            default: break;
            }
            break;
        }
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

bool Window::mouse_captured() const { return SDL_GetWindowRelativeMouseMode(impl_->window); }

void Window::set_text_input(bool enabled) {
    if (enabled == SDL_TextInputActive(impl_->window)) return;
    if (enabled) SDL_StartTextInput(impl_->window);
    else SDL_StopTextInput(impl_->window);
}

float Window::pixel_density() const {
    const float d = SDL_GetWindowPixelDensity(impl_->window);
    return d > 0.0f ? d : 1.0f;
}

std::string Window::clipboard_text() const {
    char* text = SDL_GetClipboardText();
    if (!text) return {};
    std::string out(text);
    SDL_free(text);
    return out;
}

void Window::set_clipboard_text(std::string_view utf8) {
    const std::string copy(utf8);
    SDL_SetClipboardText(copy.c_str());
}

void Window::set_cursor(CursorShape shape) {
    const auto idx = static_cast<std::size_t>(shape);
    if (idx >= impl_->cursors.size() || impl_->active_cursor == shape) return;
    if (!impl_->cursors[idx]) {
        SDL_SystemCursor sys = SDL_SYSTEM_CURSOR_DEFAULT;
        switch (shape) {
        case CursorShape::TextInput: sys = SDL_SYSTEM_CURSOR_TEXT; break;
        case CursorShape::ResizeAll: sys = SDL_SYSTEM_CURSOR_MOVE; break;
        case CursorShape::ResizeNS: sys = SDL_SYSTEM_CURSOR_NS_RESIZE; break;
        case CursorShape::ResizeEW: sys = SDL_SYSTEM_CURSOR_EW_RESIZE; break;
        case CursorShape::ResizeNESW: sys = SDL_SYSTEM_CURSOR_NESW_RESIZE; break;
        case CursorShape::ResizeNWSE: sys = SDL_SYSTEM_CURSOR_NWSE_RESIZE; break;
        case CursorShape::Hand: sys = SDL_SYSTEM_CURSOR_POINTER; break;
        case CursorShape::NotAllowed: sys = SDL_SYSTEM_CURSOR_NOT_ALLOWED; break;
        default: break;
        }
        impl_->cursors[idx] = SDL_CreateSystemCursor(sys);
    }
    if (impl_->cursors[idx]) SDL_SetCursor(impl_->cursors[idx]);
    impl_->active_cursor = shape;
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
