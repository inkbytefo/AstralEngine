#pragma once

#include <unordered_map>
#include <bitset>
#include <cstdint>

namespace AstralEngine {

class Window;

// Desteklenen tuş kodları (SDL3 keycodes ile eşleşecek)
enum class KeyCode {
    Unknown = 0,
    
    // Letters
    A = 4, B = 5, C = 6, D = 7, E = 8, F = 9, G = 10, H = 11, I = 12, J = 13,
    K = 14, L = 15, M = 16, N = 17, O = 18, P = 19, Q = 20, R = 21, S = 22,
    T = 23, U = 24, V = 25, W = 26, X = 27, Y = 28, Z = 29,
    
    // Numbers
    Number1 = 30, Number2 = 31, Number3 = 32, Number4 = 33, Number5 = 34,
    Number6 = 35, Number7 = 36, Number8 = 37, Number9 = 38, Number0 = 39,
    
    // Special keys
    Return = 40, Escape = 41, Backspace = 42, Tab = 43, Space = 44,
    
    // Arrow keys
    Right = 79, Left = 80, Down = 81, Up = 82,
    
    // Function keys
    F1 = 58, F2 = 59, F3 = 60, F4 = 61, F5 = 62, F6 = 63,
    F7 = 64, F8 = 65, F9 = 66, F10 = 67, F11 = 68, F12 = 69,
    
    // Modifier keys
    LeftCtrl = 224, LeftShift = 225, LeftAlt = 226,
    RightCtrl = 228, RightShift = 229, RightAlt = 230,
    
    // Sayının maksimum değeri
    MAX_KEYS = 512
};

// Mouse butonları
enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4,
    
    MAX_BUTTONS = 8
};

/**
 * @brief Kullanıcı girdilerini yöneten sınıf.
 * 
 * Klavye, fare ve diğer girdi cihazlarından gelen
 * verileri işler ve oyun sistemlerine sunar.
 */
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Non-copyable
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Yaşam döngüsü
    bool Initialize(Window* window);
    void Update();
    void Shutdown();

    // Klavye girdi sorguları
    bool IsKeyPressed(KeyCode key) const;
    bool IsKeyJustPressed(KeyCode key) const;  // Bu frame'de basıldı mı?
    bool IsKeyJustReleased(KeyCode key) const; // Bu frame'de bırakıldı mı?

    // Mouse girdi sorguları  
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonJustPressed(MouseButton button) const;
    bool IsMouseButtonJustReleased(MouseButton button) const;
    
    // Mouse pozisyonu ve hareketi
    void GetMousePosition(int& x, int& y) const;
    void GetMouseDelta(int& dx, int& dy) const;
    int GetMouseWheelDelta() const { return m_mouseWheelDelta; }

    // Internal event handling (Platform tarafından çağrılır)
    void OnKeyEvent(KeyCode key, bool pressed);
    void OnMouseButtonEvent(MouseButton button, bool pressed);
    void OnMouseMoveEvent(int x, int y);
    void OnMouseWheelEvent(int delta);
    
    // SDL3 event handlers (PlatformSubsystem tarafından çağrılır)
    void HandleSDLKeyEvent(int sdlKeycode, bool pressed);
    void HandleSDLMouseButtonEvent(uint8_t sdlButton, bool pressed, float x, float y);
    

private:
    void ResetFrameInputs();

    // Keyboard state
    std::bitset<static_cast<size_t>(KeyCode::MAX_KEYS)> m_keyboardState;
    std::bitset<static_cast<size_t>(KeyCode::MAX_KEYS)> m_keyboardStatePrevious;
    
    // Mouse state
    std::bitset<static_cast<size_t>(MouseButton::MAX_BUTTONS)> m_mouseState;
    std::bitset<static_cast<size_t>(MouseButton::MAX_BUTTONS)> m_mouseStatePrevious;
    
    // Mouse position and movement
    int m_mouseX = 0, m_mouseY = 0;
    int m_mouseDeltaX = 0, m_mouseDeltaY = 0;
    int m_mouseWheelDelta = 0;
    
    Window* m_window = nullptr;
    bool m_initialized = false;
};

} // namespace AstralEngine
