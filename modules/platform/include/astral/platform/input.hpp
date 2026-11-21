#pragma once

#include <astral/core/math.hpp>

#include <array>
#include <string>
#include <string_view>
#include <cstdint>

namespace astral {

enum class Key : std::uint8_t {
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Space, Enter, Escape, Tab, Backspace,
    Left, Right, Up, Down,
    LeftShift, RightShift, LeftCtrl, RightCtrl, LeftAlt, RightAlt,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count
};

enum class MouseButton : std::uint8_t { Left, Right, Middle, Count };

/// Keyboard and mouse state for the current frame. Pure data: the window feeds it events,
/// game code reads it. `pressed`/`released` are true only in the frame the change happened.
class Input {
public:
    /// Starts a new frame: clears edges and per-frame deltas. Window::poll() calls this.
    void begin_frame() noexcept;

    void set_key(Key key, bool down) noexcept;
    void set_mouse_button(MouseButton button, bool down) noexcept;
    void add_mouse_motion(Vec2 position, Vec2 delta) noexcept;
    void add_scroll(Vec2 amount) noexcept;
    /// Typed text (UTF-8), for text fields. Only arrives while Window::set_text_input(true).
    void add_text(std::string_view utf8);
    /// Forget keyboard / mouse state (a UI that took the input hides it from the game).
    void clear_keyboard() noexcept;
    void clear_mouse() noexcept;

    [[nodiscard]] bool down(Key key) const noexcept;
    [[nodiscard]] bool pressed(Key key) const noexcept;
    [[nodiscard]] bool released(Key key) const noexcept;
    [[nodiscard]] bool down(MouseButton button) const noexcept;
    [[nodiscard]] bool pressed(MouseButton button) const noexcept;
    [[nodiscard]] bool released(MouseButton button) const noexcept;

    [[nodiscard]] Vec2 mouse_position() const noexcept { return mouse_position_; }
    [[nodiscard]] Vec2 mouse_delta() const noexcept { return mouse_delta_; }
    [[nodiscard]] Vec2 scroll() const noexcept { return scroll_; }
    /// Text typed this frame (UTF-8).
    [[nodiscard]] std::string_view text() const noexcept { return text_; }

    /// -1..1 per axis from four keys (opposite keys cancel).
    [[nodiscard]] Vec2 axis(Key negative_x, Key positive_x, Key negative_y, Key positive_y) const noexcept;
    /// axis(A, D, S, W): x = strafe, y = forward.
    [[nodiscard]] Vec2 wasd() const noexcept { return axis(Key::A, Key::D, Key::S, Key::W); }

private:
    struct ButtonState {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };
    static void apply(ButtonState& state, bool down) noexcept;

    std::array<ButtonState, static_cast<std::size_t>(Key::Count)> keys_{};
    std::array<ButtonState, static_cast<std::size_t>(MouseButton::Count)> mouse_{};
    Vec2 mouse_position_{};
    Vec2 mouse_delta_{};
    Vec2 scroll_{};
    std::string text_;
};

} // namespace astral
