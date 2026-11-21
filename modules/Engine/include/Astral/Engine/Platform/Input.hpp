#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace Astral {

// Key codes (values match GLFW so the window layer can index directly).
enum class Key : int {
    Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Escape = 256, Enter, Tab, Backspace, Insert, Delete, Right, Left, Down, Up, PageUp, PageDown, Home, End,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift = 340, LeftControl, LeftAlt, LeftSuper, RightShift, RightControl, RightAlt, RightSuper,
};
inline constexpr int kKeyCount = 349;

enum class MouseButton : int { Left = 0, Right = 1, Middle = 2 };

// Per-frame input state. "Pressed"/"Released" are true only in the frame the change happened.
class Input {
public:
    bool IsDown(Key key) const { return Valid(key) && m_Down[int(key)]; }
    bool WasPressed(Key key) const { return Valid(key) && m_Down[int(key)] && !m_Prev[int(key)]; }
    bool WasReleased(Key key) const { return Valid(key) && !m_Down[int(key)] && m_Prev[int(key)]; }

    bool IsDown(MouseButton b) const { return m_Mouse[int(b)]; }
    bool WasPressed(MouseButton b) const { return m_Mouse[int(b)] && !m_MousePrev[int(b)]; }
    bool WasReleased(MouseButton b) const { return !m_Mouse[int(b)] && m_MousePrev[int(b)]; }

    glm::vec2 MousePosition() const { return m_MousePos; }
    glm::vec2 MouseDelta() const { return m_MousePos - m_MousePrevPos; }
    float ScrollDelta() const { return m_Scroll; }

    // Named actions and axes, e.g. BindAxis("MoveX", Key::D, Key::A); GetAxis("MoveX") in [-1, 1].
    void BindAction(const std::string& action, Key key) { m_Actions[action].push_back({key, -1}); }
    void BindAction(const std::string& action, MouseButton button) { m_Actions[action].push_back({Key(-1), int(button)}); }
    void BindAxis(const std::string& axis, Key positive, Key negative) { m_Axes[axis].push_back({positive, negative}); }
    bool IsActionDown(const std::string& action) const;
    bool WasActionPressed(const std::string& action) const;
    float GetAxis(const std::string& axis) const;

    // Window-system side
    void BeginFrame();          // call before polling events
    void SetKey(int key, bool down) { if (key >= 0 && key < kKeyCount) m_Down[key] = down; }
    void SetMouseButton(int button, bool down) { if (button >= 0 && button < 3) m_Mouse[button] = down; }
    void SetMousePosition(glm::vec2 p) { m_MousePos = p; }
    void AddScroll(float s) { m_Scroll += s; }
    void Reset();

private:
    static bool Valid(Key k) { return int(k) >= 0 && int(k) < kKeyCount; }
    std::array<bool, kKeyCount> m_Down{}, m_Prev{};
    std::array<bool, 3> m_Mouse{}, m_MousePrev{};
    glm::vec2 m_MousePos{0.0f}, m_MousePrevPos{0.0f};
    float m_Scroll = 0.0f;
    struct ActionBinding {
        Key key;
        int mouseButton; // -1 when the binding is a key
        bool Down(const Input& in) const { return mouseButton >= 0 ? in.IsDown(MouseButton(mouseButton)) : in.IsDown(key); }
        bool Pressed(const Input& in) const { return mouseButton >= 0 ? in.WasPressed(MouseButton(mouseButton)) : in.WasPressed(key); }
    };
    struct AxisBinding { Key positive, negative; };
    std::unordered_map<std::string, std::vector<ActionBinding>> m_Actions;
    std::unordered_map<std::string, std::vector<AxisBinding>> m_Axes;
};

} // namespace Astral
