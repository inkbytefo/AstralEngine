#pragma once

#include "KeyCode.h"
#include <unordered_map>
#include <bitset>
#include <cstdint>

#ifdef ASTRAL_USE_SDL3
    #include <SDL3/SDL_keycode.h>
#endif

namespace AstralEngine {

#ifdef ASTRAL_USE_SDL3
    KeyCode SDLKeyToAstralKey(SDL_Keycode sdlKey);
    MouseButton SDLButtonToAstralButton(uint8_t sdlButton);
#endif

class Window;

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
