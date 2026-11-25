#pragma once

#include <astral/gpu/device.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace astral {

class Input;

enum class CursorShape : std::uint8_t {
    Arrow,
    TextInput,
    ResizeAll,
    ResizeNS,
    ResizeEW,
    ResizeNESW,
    ResizeNWSE,
    Hand,
    NotAllowed,
    Count,
};

struct WindowDesc {
    std::string_view title = "Astral";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool resizable = true;
    bool hidden = false;
};

/// An OS window. Pump it once per frame with poll(), which also feeds an Input.
class Window {
public:
    /// Returns nullptr if no video driver is available.
    static std::unique_ptr<Window> create(const WindowDesc& desc);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /// Processes pending OS events into `input`. Returns false once closing was requested.
    bool poll(Input& input);
    /// Answers the OS without consuming events: the window stays "responsive" while the caller is busy for a
    /// while (loading). Events stay queued for the next poll(). Cheap; call from the thread that polls.
    void keep_responsive();
    void request_close() noexcept { close_requested_ = true; }
    [[nodiscard]] bool close_requested() const noexcept { return close_requested_; }

    /// Drawable size in pixels (may differ from the logical size on high-DPI screens).
    [[nodiscard]] gpu::Extent2D size() const;
    [[nodiscard]] bool minimized() const;
    void set_title(std::string_view title);
    /// Hides the cursor and reports relative motion (FPS camera style).
    void set_mouse_captured(bool captured);
    [[nodiscard]] bool mouse_captured() const;
    /// Delivers typed text to Input::text() (and may show an on-screen keyboard).
    void set_text_input(bool enabled);
    /// Drawable pixels per window coordinate (2 on many high-DPI screens). Mouse positions are in
    /// window coordinates: multiply by this to get pixels.
    [[nodiscard]] float pixel_density() const;

    /// System clipboard UTF-8 text (empty if unavailable).
    [[nodiscard]] std::string clipboard_text() const;
    void set_clipboard_text(std::string_view utf8);
    /// Sets the OS mouse cursor shape for this window.
    void set_cursor(CursorShape shape);

    /// False under drivers without Vulkan support (e.g. SDL's headless "dummy" driver).
    [[nodiscard]] bool supports_vulkan() const noexcept;

    /// Lets gpu::Device and gpu::Swapchain render into this window. Without Vulkan support
    /// only drawable_size is set.
    [[nodiscard]] gpu::SurfaceSource surface_source() const;

private:
    struct Impl;
    explicit Window(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
    bool close_requested_ = false;
};

} // namespace astral
